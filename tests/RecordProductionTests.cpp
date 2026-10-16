#include <atomic>
#include <cassert>
#include <chrono>
#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"
#include <logging/RecordProduction.hpp>
namespace {
namespace L=ESPressio::Logging;
namespace C=ESPressio::Clock;
namespace T=ESPressio::Threading;
namespace M=ESPressio::Memory;
struct Rec {
 static std::atomic<int> Live;
 std::uint32_t Ticket; std::uint64_t Time; int Value;
 Rec(L::AttemptSequence<> s,L::QualifiedLogTime t,int v) noexcept:
  Ticket(s.Value()),Time(t.Monotonic.Nanoseconds()),Value(v){++Live;}
 ~Rec() noexcept {--Live;}
};
std::atomic<int> Rec::Live{0};
using Top=M::MemoryTopology<Resource,M::SharedReserve<0U,Resource>,
 M::ObjectPoolSpec<Rec,M::DedicatedInstances<2U>>>;
using Runtime2=M::MemoryRuntime<Top,MemoryComposition,
 TestSupport::TestMutexProvider,TestSupport::TestSignalProvider>;
using Pool=Runtime2::ObjectPoolType<Rec>; using Lease=Pool::LeaseType;
using Wake=T::BoundedWaitWakeProvider<TestSupport::TestSignalProvider,3U>;
using Life=L::AdmissionLifecycleCoordinator<2U,Wake>;
struct Q{};struct S{};
using Mutex=T::OrdinaryMutexProvider<Q,TestSupport::TestMutexProvider>;
using Sequencer=T::OrdinaryMutexProvider<S,TestSupport::TestMutexProvider>;
using Queue=L::TransactionalRecordQueue<2U,Lease,Mutex,Life>;
struct Temporal {
 mutable int Calls{};
 L::CoherentLogClockSample CaptureCoherent() const noexcept {
  ++Calls;
  return {C::MonotonicNow(),{C::SynchronizedTimestamp::FromNanoseconds(0U),
   C::SynchronizationUncertainty::Maximum(),C::SynchronizationState::NeverSynchronized},
   C::ClockEra(),false};
 }
};
void Test() {
 Resource resource; TestSupport::TestMutexProvider native;
 MemoryComposition::Select<M::SharedReserveAllocationRequirement,
 ESPressio::System::CompositionFramework::SelectUnique> allocator;
 Runtime2 runtime(native,allocator,resource);
 M::MemoryTopologyInitializationFailure failure{};
 assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
 Wake wake; Life life(wake); Mutex mutex;Queue queue(mutex,life);
 Sequencer seqMutex; L::AttemptSequenceCounter<> seq;Temporal capture;
 auto& pool=runtime.ObjectPoolFor<Rec>();
 const auto running=[]() noexcept -> bool{return false;};
 const auto produce=[&](int v,std::size_t slot,Queue::Reservation& out,Lease& tmp){
  return L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,8U,8U>(
   seq,seqMutex,capture,queue,life,wake,slot,pool,tmp,out,
   DeadlineAfter(std::chrono::milliseconds(1000)),running,v);
 };
 struct Denied {
  T::OrdinaryMutexAcquireResult Acquire(ESPressio::Platform::Synchronization::WaitTimeout) noexcept {
   return T::OrdinaryMutexAcquireResult::ProviderFailure;
  }
  T::OrdinaryMutexReleaseResult Release() noexcept {
   return T::OrdinaryMutexReleaseResult::Released;
  }
 } denied;
 Lease emptyScratch;Queue::Reservation emptyOwner;
 const auto refused=L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,2U,2U>(
  seq,denied,capture,queue,life,wake,0U,pool,emptyScratch,emptyOwner,
  DeadlineAfter(std::chrono::milliseconds(100)),running,1);
 assert(refused.Status==L::RecordProductionStatus::SequencerProviderFailure);
 assert(!refused.Attempt.IsValid()&&capture.Calls==0&&seq.LastIssued()==0U);
 assert(emptyScratch.IsEmpty()&&!emptyOwner.IsActive());
 struct BrokenRelease {
  T::OrdinaryMutexAcquireResult Acquire(ESPressio::Platform::Synchronization::WaitTimeout) noexcept {
   return T::OrdinaryMutexAcquireResult::Acquired;
  }
  T::OrdinaryMutexReleaseResult Release() noexcept {
   return T::OrdinaryMutexReleaseResult::ProviderFailure;
  }
 } broken;
 L::AttemptSequenceCounter<> isolated;
 const auto uncertain=L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,2U,2U>(
  isolated,broken,capture,queue,life,wake,0U,pool,emptyScratch,emptyOwner,
  DeadlineAfter(std::chrono::milliseconds(100)),running,2);
 assert(uncertain.Status==L::RecordProductionStatus::SequencerReleaseUnconfirmed);
 assert(uncertain.Attempt.Value()==1U&&isolated.LastIssued()==1U);
 assert(capture.Calls==1&&Rec::Live.load()==0);
 capture.Calls=0;
 Lease scratch;Queue::Reservation reservation;
 const auto first=produce(41,0U,reservation,scratch);
 assert(first.Status==L::RecordProductionStatus::Published&&first.Attempt.Value()==1U);
 assert(first.TimeCaptured&&capture.Calls==1&&!reservation.IsActive()&&scratch.IsEmpty());
 Queue::ConsumerClaim claim;int observed{};
 const auto ok=[&](const Rec& r) noexcept -> L::RecordSinkSubmission{
  assert(r.Value==41&&r.Time>0U);observed=r.Ticket;return L::RecordSinkSubmission::Accepted;};
 auto drained=L::DrainOneRecord(queue,claim,ok);
 assert(drained.Status==L::RecordDrainStatus::AcceptedAndReclaimed);
 assert(observed==1&&!claim.IsActive()&&Rec::Live.load()==0);
 Lease t2;Queue::Reservation r2;
 const auto second=produce(42,1U,r2,t2);
 assert(second.Status==L::RecordProductionStatus::Published&&second.Attempt.Value()==2U);
 Queue::ConsumerClaim c2;
 const auto no=[](const Rec&) noexcept -> L::RecordSinkSubmission{return L::RecordSinkSubmission::Rejected;};
 drained=L::DrainOneRecord(queue,c2,no);
 assert(drained.Status==L::RecordDrainStatus::FailedAndReclaimed);
 assert(drained.Submission==L::RecordSinkSubmission::Rejected&&Rec::Live.load()==0);
 using Timeout=ESPressio::Platform::Synchronization::WaitTimeout;
 Queue::Reservation hold0,hold1;
 assert(queue.ReserveSlot(hold0,Timeout::NoWait())==L::RecordQueueResult::Succeeded);
 assert(queue.ReserveSlot(hold1,Timeout::NoWait())==L::RecordQueueResult::Succeeded);
 Lease t3;Queue::Reservation r3;
 const auto failed=L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,1U,1U>(
  seq,seqMutex,capture,queue,life,wake,0U,pool,t3,r3,
  DeadlineAfter(std::chrono::milliseconds(5)),running,43);
 assert(failed.Status==L::RecordProductionStatus::AdmissionFailed);
 assert(failed.Attempt.Value()==3U&&failed.TimeCaptured&&capture.Calls==3);
 assert(t3.IsEmpty()&&!r3.IsActive()&&Rec::Live.load()==0);
 assert(hold0.Abort()==L::RecordQueueResult::Succeeded);
 assert(hold1.Abort()==L::RecordQueueResult::Succeeded);
 Lease t4;Queue::Reservation r4;
 const auto invalid=produce(44,2U,r4,t4);
 assert(invalid.Status==L::RecordProductionStatus::InvalidProducerSlot);
 assert(seq.LastIssued()==3U&&capture.Calls==3);
 assert(life.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status==
  L::AdmissionShutdownStatus::Quiescent);
 const auto shutdown=produce(45,0U,r4,t4);
 assert(shutdown.Status==L::RecordProductionStatus::StopRequested);
 assert(seq.LastIssued()==3U&&Rec::Live.load()==0);
 assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
}
}
int main(){static CanonicalClock clock;assert(C::BindMonotonicClock(clock));Test();}
