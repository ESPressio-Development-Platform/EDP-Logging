# Basic synchronous console integration

The root [consumer walkthrough](../README.MD#getting-started) contains every
project file for a PIOArduino/Arduino-ESP32 application using existing Logging
source assembly and full generated typed JSON projection. This document owns
the profile boundaries; the tutorial contains the complete source.

## Bootstrap and identity

One ordinary setup context establishes the hardware-derived tutorial device
namespace and commits a nonzero, nonwrapping NVS incarnation before constructing
the address-stable SystemIdentity. Missing metadata starts at one. Read, decode,
capacity, native or commit uncertainty stops record emission without automatic
erase, counter reset or retry. A product must use its authoritative device
identity and one incarnation authority, and must not reuse old identity tuples
after erasing the tutorial namespace. Startup is single-context; concurrent
boots/producers must not independently advance the same identity counter.

The application selects existing native MonotonicTimebase and Clock providers
through a checked Composition architecture and binds one canonical Clock.
Source observations read only that Clock. No synchronized service is present,
so unavailable correlation, invalid Era and maximum uncertainty are preserved
with the local occurrence coordinate. No synchronization quality is invented.

## Capture and resource ownership

The entire enabled site is lazily gated. The single logger owns a 32-bit
nonwrapping attempt counter; disabled sites do not evaluate arguments or issue
tickets. A record owns one exact uint32 argument in four arena bytes. The
Localisation owner generates the descriptor/fingerprint for message 1/0/3.
Assembly and projection must succeed before the existing Serialisation owner
encodes all fifteen fields into 1024 fixed bytes. The synchronous adapter
introduces no record queue, heap allocator, worker or persistence archive.

Static identity/Clock/logger owners survive Arduino setup. Temporary source,
projection and output buffers remain alive through synchronous console calls;
no borrowed record pointer escapes. The adapter is not thread-safe.

## Console acceptance and failure

Arduino Serial is an explicitly selected application console. It may block
under the chosen core's native behavior. Complete write counts plus a newline
mean console acceptance only. There is no hard deadline, peer acknowledgement,
physical-wire completion or durable log claim. Partial output may already be
visible; the example stops without blindly resubmitting or mixing another
record into that uncertain stream. Native bootstrap error text is separate
from generated LogRecords. Error status is checked outside the gate lambda,
so returning from a lambda cannot accidentally report application success.

For concurrent producers or asynchronous service, compose the existing bounded
admission, Memory leases, coordinator and typed sinks. For durable records,
use finite acknowledged archive capacity and separately qualified persistence
providers. The identity NVS counter is not a durable log sink.

## Build and validation scope

The manifest declares GitHub URLs for all selected source owners. The native
Platform pin supplies the existing timebase; its Security package dependency
is a package-resolution edge, not a new Logging runtime dependency. Schema
generation uses the downloaded Localisation owner's complete source bundle
and public compiler, never Logging's HIL/test generator. No sibling checkout,
extra partition image, filesystem upload or external Python package is needed.

Verification must extract the README's exact source blocks and compare them
with the application being built. Firmware link, scripted host behavior and
physical device execution are distinct evidence scopes.
