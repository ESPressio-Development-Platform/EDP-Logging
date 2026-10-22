#include <atomic>
#include <cassert>
#include <chrono>
#define EDP_LOGGING_G1_LIFECYCLE_TEST_INCLUDE
#include "AdmissionWakeCoordinatorTests.cpp"
#include <logging/RecordProduction.hpp>
#include <logging/CoordinatorQuantum.hpp>
#include <logging/DedicatedCoordinator.hpp>
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
 // The enabled occurrence must already be captured even if the sequencer
 // lock subsequently refuses the attempt.
 assert(!refused.Attempt.IsValid()&&refused.TimeCaptured&&
  capture.Calls==1&&seq.LastIssued()==0U);
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
 assert(capture.Calls==2&&Rec::Live.load()==0);
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
 assert(seq.LastIssued()==3U&&capture.Calls==4);
 assert(life.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status==
  L::AdmissionShutdownStatus::Quiescent);
 const auto shutdown=produce(45,0U,r4,t4);
 assert(shutdown.Status==L::RecordProductionStatus::StopRequested);
 assert(shutdown.TimeCaptured&&capture.Calls==5);
 assert(seq.LastIssued()==3U&&Rec::Live.load()==0);
 assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
}

// Regression: a contended sequencer is a real wait. The producer MUST
// snapshot both occurrence and caller state before calling Acquire.
void TestOccurrenceAndArgumentSnapshot() {
 Resource resource; TestSupport::TestMutexProvider native;
 MemoryComposition::Select<M::SharedReserveAllocationRequirement,
 ESPressio::System::CompositionFramework::SelectUnique> allocator;
 Runtime2 runtime(native,allocator,resource);
 M::MemoryTopologyInitializationFailure failure{};
 assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
 Wake wake; Life life(wake); Mutex mutex; Queue queue(mutex,life);
 Temporal clock; L::AttemptSequenceCounter<> sequence;
 auto& pool=runtime.ObjectPoolFor<Rec>();
 int sourceValue=81;
 struct MutatingSequencer {
  int& Source; const Temporal& Clock; bool Observed=false;
  T::OrdinaryMutexAcquireResult Acquire(
   ESPressio::Platform::Synchronization::WaitTimeout
  ) noexcept {
   // An occurrence sampled after this call would be semantically wrong.
   assert(Clock.Calls==1);
   Observed=true;
   Source=999;
   return T::OrdinaryMutexAcquireResult::Acquired;
  }
  T::OrdinaryMutexReleaseResult Release() noexcept {
   return T::OrdinaryMutexReleaseResult::Released;
  }
 } sequencer{sourceValue,clock};
 auto running=[]() noexcept -> bool {return false;};
 Lease scratch; Queue::Reservation reservation;
 const auto out=L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,4U,4U>(
  sequence,sequencer,clock,queue,life,wake,0U,pool,scratch,reservation,
  DeadlineAfter(std::chrono::milliseconds(1000)),running,sourceValue);
 assert(out.Status==L::RecordProductionStatus::Published);
 assert(out.TimeCaptured && clock.Calls==1 && sequencer.Observed);
 assert(sourceValue==999);
 Queue::ConsumerClaim claim;
 const auto sink=[](const Rec& record) noexcept -> L::RecordSinkSubmission {
  assert(record.Value==81);
  assert(record.Ticket==1U && record.Time!=0U);
  return L::RecordSinkSubmission::Accepted;
 };
 const auto drain=L::DrainOneRecord(queue,claim,sink);
 assert(drain.Status==L::RecordDrainStatus::AcceptedAndReclaimed);
 assert(Rec::Live.load()==0);
 assert(life.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status==
  L::AdmissionShutdownStatus::Quiescent);
 assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
}

void TestFanout() {
 Resource resource; TestSupport::TestMutexProvider native;
 MemoryComposition::Select<M::SharedReserveAllocationRequirement,
 ESPressio::System::CompositionFramework::SelectUnique> allocator;
 Runtime2 runtime(native,allocator,resource);
 M::MemoryTopologyInitializationFailure failure{};
 assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
 Wake wake; Life life(wake); Mutex mutex; Queue queue(mutex,life);
 Sequencer seqMutex; L::AttemptSequenceCounter<> sequence; Temporal clock;
 auto& pool=runtime.ObjectPoolFor<Rec>();
 const auto running=[]() noexcept -> bool { return false; };
 const auto produce=[&](int value) {
  Lease scratch; Queue::Reservation reservation;
  const auto out=L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,8U,8U>(
   sequence,seqMutex,clock,queue,life,wake,0U,pool,scratch,reservation,
   DeadlineAfter(std::chrono::milliseconds(1000)),running,value);
  assert(out.Status==L::RecordProductionStatus::Published);
  assert(scratch.IsEmpty()&&!reservation.IsActive());
 };
 int first=0,second=0,third=0;
 auto ok1=[&](const Rec& rec) noexcept -> L::RecordSinkSubmission {
  ++first; assert(rec.Value==909);return L::RecordSinkSubmission::Accepted;
 };
 auto ok2=[&](const Rec& rec) noexcept -> L::RecordSinkSubmission {
  ++second; assert(rec.Value==909);return L::RecordSinkSubmission::Accepted;
 };
 Queue::ConsumerClaim claim;
 auto empty=L::DrainOneRecordToSinks(queue,claim,ok1,ok2);
 assert(empty.Source.Status==L::RecordDrainStatus::Empty &&
  empty.Attempted==0U&&first==0&&second==0);
 produce(909);
 auto full=L::DrainOneRecordToSinks(queue,claim,ok1,ok2);
 assert(full.FullySubmittedAndReclaimed()&&
  full.Submissions[0]==L::RecordSinkSubmission::Accepted&&
  full.Submissions[1]==L::RecordSinkSubmission::Accepted);
 assert(full.Attempted==2U&&full.Accepted==2U&&full.Unaccepted==0U);
 assert(!claim.IsActive()&&Rec::Live.load()==0);
 assert(first==1&&second==1);
 // Sink 1 commits but sink 2 rejects. No duplicate invocation/retry.
 produce(910);
 auto sinkAccepted=[&](const Rec& rec) noexcept -> L::RecordSinkSubmission {
  ++first;assert(rec.Value==910);return L::RecordSinkSubmission::Accepted;};
 auto sinkReject=[&](const Rec& rec) noexcept -> L::RecordSinkSubmission {
  ++second;assert(rec.Value==910);return L::RecordSinkSubmission::Rejected;};
 auto mixed=L::DrainOneRecordToSinks(queue,claim,sinkAccepted,sinkReject);
 assert(!mixed.FullySubmittedAndReclaimed()&&
  mixed.Source.Status==L::RecordDrainStatus::FailedAndReclaimed);
 assert(mixed.Source.Submission==L::RecordSinkSubmission::Partial);
 assert(mixed.Attempted==2U&&mixed.Accepted==1U&&mixed.Unaccepted==1U);
 assert(first==2&&second==2&&Rec::Live.load()==0);
 // Every sink receives one call on failure and is independently observed.
 produce(911);
 auto denied=[&](const Rec&) noexcept -> L::RecordSinkSubmission {
  ++first;return L::RecordSinkSubmission::Rejected;};
 auto partial=[&](const Rec&) noexcept -> L::RecordSinkSubmission {
  ++second;return L::RecordSinkSubmission::Partial;};
 auto fail=[&](const Rec&) noexcept -> L::RecordSinkSubmission {
  ++third;return L::RecordSinkSubmission::ProviderFailure;};
 auto errors=L::DrainOneRecordToSinks(queue,claim,denied,partial,fail);
 assert(errors.Attempted==3U&&errors.Accepted==0U&&errors.Unaccepted==3U);
 assert(errors.Source.Status==L::RecordDrainStatus::FailedAndReclaimed &&
  errors.Source.Submission==L::RecordSinkSubmission::Partial);
 assert(errors.Submissions[0]==L::RecordSinkSubmission::Rejected &&
  errors.Submissions[1]==L::RecordSinkSubmission::Partial &&
  errors.Submissions[2]==L::RecordSinkSubmission::ProviderFailure);
 assert(first==3&&second==3&&third==1&&Rec::Live.load()==0);
 produce(912);
 assert(queue.Pop(claim)==L::RecordQueueResult::Succeeded);
 auto occupied=L::DrainOneRecordToSinks(queue,claim,denied,partial);
 assert(occupied.Attempted==0U&&
  occupied.Source.Pop==L::RecordQueueResult::OutputHandleOccupied);
 assert(claim.IsActive()&&claim.Complete()==L::RecordQueueResult::Succeeded);
 assert(Rec::Live.load()==0);
 assert(life.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status==
  L::AdmissionShutdownStatus::Quiescent);
 assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
}

void TestBoundedCoordinatorQuantum() {
 Resource resource; TestSupport::TestMutexProvider native;
 MemoryComposition::Select<M::SharedReserveAllocationRequirement,
 ESPressio::System::CompositionFramework::SelectUnique> allocator;
 Runtime2 runtime(native,allocator,resource);
 M::MemoryTopologyInitializationFailure failure{};
 assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
 Wake wake; Life life(wake); Mutex mutex; Queue queue(mutex,life);
 Sequencer sequencer; L::AttemptSequenceCounter<> seq; Temporal time;
 auto& pool=runtime.ObjectPoolFor<Rec>();
 const auto running=[]() noexcept -> bool{return false;};
 const auto produce=[&](int value) {
  Lease scratch; Queue::Reservation reservation;
  const auto r=L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,4U,4U>(
   seq,sequencer,time,queue,life,wake,0U,pool,scratch,reservation,
   DeadlineAfter(std::chrono::milliseconds(200)),running,value);
  assert(r.Status==L::RecordProductionStatus::Published);
 };
 int first=0,second=0;
 auto one=[&](const Rec& record) noexcept -> L::RecordSinkSubmission {
  assert(record.Value==first+1);
  ++first;
  return record.Value==2 ? L::RecordSinkSubmission::Rejected
                         : L::RecordSinkSubmission::Accepted;
 };
 auto two=[&](const Rec& record) noexcept -> L::RecordSinkSubmission {
  assert(record.Value==second+1);
  ++second;
  return L::RecordSinkSubmission::Accepted;
 };
 Queue::ConsumerClaim claim;
 auto idle=L::DrainCoordinatorQuantum<1U>(queue,claim,one,two);
 assert(idle.Status==L::CoordinatorQuantumStatus::QueueEmpty);
 assert(idle.Processed==0U);
 produce(1);
 produce(2);
 const auto firstPass=L::DrainCoordinatorQuantum<1U>(queue,claim,one,two);
 assert(firstPass.Status==L::CoordinatorQuantumStatus::BudgetConsumed);
 assert(firstPass.Processed==1U && firstPass.AcceptedByAll==1U);
 assert(firstPass.PerSinkAccepted[0]==1U&&firstPass.PerSinkAccepted[1]==1U);
 assert(Rec::Live.load()==1);
 produce(3);
 const auto secondPass=L::DrainCoordinatorQuantum<2U>(queue,claim,one,two);
 assert(secondPass.Status==L::CoordinatorQuantumStatus::BudgetConsumed);
 assert(secondPass.Processed==2U && secondPass.AcceptedByAll==1U);
 assert(secondPass.FailedSubmissionRecords==1U);
 assert(secondPass.PerSinkAccepted[0]==1U && secondPass.PerSinkUnaccepted[0]==1U);
 assert(secondPass.PerSinkAccepted[1]==2U && secondPass.PerSinkUnaccepted[1]==0U);
 assert(first==3 && second==3 && Rec::Live.load()==0);
 const auto drained=L::DrainCoordinatorQuantum<8U>(queue,claim,one,two);
 assert(drained.Status==L::CoordinatorQuantumStatus::QueueEmpty &&
  drained.Processed==0U);
 produce(4);
 assert(queue.Pop(claim)==L::RecordQueueResult::Succeeded);
 const auto prior=L::DrainCoordinatorQuantum<1U>(queue,claim,one,two);
 assert(prior.Status==L::CoordinatorQuantumStatus::PriorClaimUnresolved);
 assert(prior.Processed==0U && first==3 && second==3);
 assert(claim.IsActive() &&
  claim.Complete()==L::RecordQueueResult::Succeeded);
 assert(Rec::Live.load()==0);
 assert(life.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status==
  L::AdmissionShutdownStatus::Quiescent);
 assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
}

// EDP-Threading public ThreadContext is supplied by a real topology at
// runtime. Deterministic test operations simulate precisely its wake/stop
// contract without manufacturing any alternate Scheduling API.
struct CoordinatorContextState {
 bool Stop{false};
 bool FailWake{false};
 int Waits{0};
 int RecordsSeenBeforeIdle{0};
};
static constexpr T::Detail::ThreadContextOperations TestContextOps{
 [](const void* opaque) noexcept -> bool {
  return static_cast<const CoordinatorContextState*>(opaque)->Stop;
 },
 [](void*) noexcept -> T::ThreadWaitResult {
  return T::ThreadWaitResult::ProviderFailure;
 },
 [](void* opaque,C::Duration duration) noexcept -> T::ThreadWaitResult {
  auto& state=*static_cast<CoordinatorContextState*>(opaque);
  assert(duration==C::Duration::FromNanoseconds(5'000'000));
  ++state.Waits;
  if(state.FailWake) return T::ThreadWaitResult::ProviderFailure;
  // Bounded worker must never wait while native source records remain.
  assert(state.RecordsSeenBeforeIdle==3);
  state.Stop=true;
  return T::ThreadWaitResult::TimedOut;
 },
 [](void*,C::MonotonicTimestamp) noexcept -> T::ThreadWaitResult {
  return T::ThreadWaitResult::ProviderFailure;
 }
};

void TestDedicatedCoordinatorCallable() {
 Resource resource;TestSupport::TestMutexProvider native;
 MemoryComposition::Select<M::SharedReserveAllocationRequirement,
 ESPressio::System::CompositionFramework::SelectUnique> allocator;
 Runtime2 runtime(native,allocator,resource);
 M::MemoryTopologyInitializationFailure failure{};
 assert(runtime.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
 Wake wake; Life life(wake); Mutex mutex; Queue queue(mutex,life);
 Sequencer sequenceMutex;L::AttemptSequenceCounter<> sequence;Temporal time;
 auto& pool=runtime.ObjectPoolFor<Rec>();
 const auto running=[]() noexcept -> bool{return false;};
 int consumed=0;int accepted=0;
 auto sinkA=[&](const Rec& rec) noexcept -> L::RecordSinkSubmission {
  assert(rec.Value==consumed+1);++consumed;
  return rec.Value==2 ? L::RecordSinkSubmission::Rejected
                      : L::RecordSinkSubmission::Accepted;
 };
 auto sinkB=[&](const Rec& rec) noexcept -> L::RecordSinkSubmission {
  assert(rec.Value==accepted+1);++accepted;
  return L::RecordSinkSubmission::Accepted;
 };
 L::DedicatedCoordinatorReport report;
 using Callable=L::DedicatedCoordinator<1U,5'000'000LL,Queue,
  decltype(sinkA),decltype(sinkB)>;
 static_assert(std::is_move_constructible_v<Callable>);
 struct DedicatedIdentity final {};
 static_assert(std::is_same_v<
  decltype(T::BindDedicatedThread<DedicatedIdentity>(std::declval<Callable>())),
  T::DedicatedThreadBinding<DedicatedIdentity,Callable>
 >);
 Callable worker(queue,report,sinkA,sinkB);
 for(int i=1;i<=2;++i){
  Lease scratch;Queue::Reservation reservation;
  const auto out=L::ProduceRecordUntil<L::AttemptSequenceWidth::Bits32,4U,4U>(
   sequence,sequenceMutex,time,queue,life,wake,0U,pool,scratch,reservation,
   DeadlineAfter(std::chrono::milliseconds(400)),running,i);
  assert(out.Status==L::RecordProductionStatus::Published);
 }
 CoordinatorContextState state{};
 state.RecordsSeenBeforeIdle=3; // sink observation below validates actual count.
 T::ThreadContext ctx(&state,TestContextOps);
 worker(ctx);
 assert(state.Waits==1 && state.Stop);
 assert(consumed==2 && accepted==2 && Rec::Live.load()==0);
 assert(report.Exit.load()==L::DedicatedCoordinatorExit::CooperativeStop);
 assert(report.RecordsRetired.load()==2U);
 assert(report.AllSinksAccepted.load()==1U);
 assert(report.PartialSubmissions.load()==1U);
 assert(report.IdleTimeouts.load()==1U);
 assert(life.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status==
  L::AdmissionShutdownStatus::Quiescent);
 assert(runtime.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);

 // A failed topology wake must terminate rather than spin or fake quiescence.
 Resource resource2;
 Runtime2 runtime2(native,allocator,resource2);
 assert(runtime2.Initialize(failure)==M::MemoryTopologyInitializationResult::Succeeded);
 Wake wake2;Life life2(wake2);Mutex mutex2;Queue queue2(mutex2,life2);
 L::DedicatedCoordinatorReport report2;
 Callable wakeFailureWorker(queue2,report2,sinkA,sinkB);
 CoordinatorContextState failureState{};
 failureState.FailWake=true;
 T::ThreadContext failureCtx(&failureState,TestContextOps);
 wakeFailureWorker(failureCtx);
 assert(failureState.Waits==1);
 assert(report2.Exit.load()==L::DedicatedCoordinatorExit::WakeProviderFailure);
 assert(report2.RecordsRetired.load()==0U);
 assert(life2.ShutdownUntil<8U>(DeadlineAfter(std::chrono::milliseconds(300))).Status==
  L::AdmissionShutdownStatus::Quiescent);
 assert(runtime2.TearDown()==M::MemoryTopologyTeardownResult::Succeeded);
}

void TestSingleSinkCoordinatorPipeline() {
    Resource resource;
    TestSupport::TestMutexProvider native;
    MemoryComposition::Select<
        M::SharedReserveAllocationRequirement,
        ESPressio::System::CompositionFramework::SelectUnique
    > allocator;
    Runtime2 runtime(native, allocator, resource);
    M::MemoryTopologyInitializationFailure failure{};
    assert(runtime.Initialize(failure) == M::MemoryTopologyInitializationResult::Succeeded);

    Wake wake;
    Life life(wake);
    Mutex mutex;
    Queue queue(mutex, life);
    Sequencer sequencer;
    L::AttemptSequenceCounter<> sequence;
    Temporal time;
    auto& pool = runtime.ObjectPoolFor<Rec>();
    const auto running = []() noexcept -> bool { return false; };
    const auto produce = [&](int value) {
        Lease scratch;
        Queue::Reservation reservation;
        const auto result = L::ProduceRecordUntil<
            L::AttemptSequenceWidth::Bits32, 4U, 4U
        >(
            sequence, sequencer, time, queue, life, wake, 0U, pool,
            scratch, reservation,
            DeadlineAfter(std::chrono::milliseconds(200)), running, value
        );
        assert(result.Status == L::RecordProductionStatus::Published);
        assert(scratch.IsEmpty() && !reservation.IsActive());
    };

    int observed = 0;
    auto soleSink = [&](const Rec& record) noexcept -> L::RecordSinkSubmission {
        assert(record.Value == observed + 1);
        ++observed;
        return (record.Value % 2) == 0
            ? L::RecordSinkSubmission::Rejected
            : L::RecordSinkSubmission::Accepted;
    };

    Queue::ConsumerClaim claim;
    const auto empty = L::DrainOneRecordToSinks(queue, claim, soleSink);
    assert(empty.Source.Status == L::RecordDrainStatus::Empty);
    assert(empty.Attempted == 0U && observed == 0);

    produce(1);
    const auto first = L::DrainOneRecordToSinks(queue, claim, soleSink);
    assert(first.FullySubmittedAndReclaimed());
    assert(first.Attempted == 1U && first.Accepted == 1U);
    assert(first.Submissions[0] == L::RecordSinkSubmission::Accepted);
    assert(!claim.IsActive() && Rec::Live.load() == 0);

    produce(2);
    const auto rejected = L::DrainCoordinatorQuantum<1U>(queue, claim, soleSink);
    assert(rejected.Status == L::CoordinatorQuantumStatus::BudgetConsumed);
    assert(rejected.Processed == 1U && rejected.AcceptedByAll == 0U);
    assert(rejected.FailedSubmissionRecords == 1U);
    assert(rejected.PerSinkAccepted[0] == 0U && rejected.PerSinkUnaccepted[0] == 1U);
    assert(!claim.IsActive() && Rec::Live.load() == 0);
    assert(L::DrainCoordinatorQuantum<1U>(queue, claim, soleSink).Status ==
        L::CoordinatorQuantumStatus::QueueEmpty);

    produce(3);
    produce(4);
    L::DedicatedCoordinatorReport report;
    using Callable = L::DedicatedCoordinator<
        1U, 5'000'000LL, Queue, decltype(soleSink)
    >;
    static_assert(std::is_move_constructible_v<Callable>);
    struct SingleSinkDedicatedIdentity final {};
    static_assert(std::is_same_v<
        decltype(T::BindDedicatedThread<SingleSinkDedicatedIdentity>(
            std::declval<Callable>())),
        T::DedicatedThreadBinding<SingleSinkDedicatedIdentity, Callable>
    >);
    Callable worker(queue, report, soleSink);
    CoordinatorContextState state{};
    state.RecordsSeenBeforeIdle = 3;
    T::ThreadContext context(&state, TestContextOps);
    worker(context);
    assert(state.Waits == 1 && state.Stop);
    assert(observed == 4 && Rec::Live.load() == 0);
    assert(report.Exit.load() == L::DedicatedCoordinatorExit::CooperativeStop);
    assert(report.RecordsRetired.load() == 2U);
    assert(report.AllSinksAccepted.load() == 1U);
    assert(report.PartialSubmissions.load() == 1U);
    assert(report.IdleTimeouts.load() == 1U);
    assert(report.NotificationFailures.load() == 0U);
    assert(life.ShutdownUntil<8U>(
        DeadlineAfter(std::chrono::milliseconds(300))
    ).Status == L::AdmissionShutdownStatus::Quiescent);
    assert(runtime.TearDown() == M::MemoryTopologyTeardownResult::Succeeded);
}

}
int main(){static CanonicalClock clock;assert(C::BindMonotonicClock(clock));Test();TestOccurrenceAndArgumentSnapshot();TestFanout();TestBoundedCoordinatorQuantum();TestDedicatedCoordinatorCallable();TestSingleSinkCoordinatorPipeline();}
