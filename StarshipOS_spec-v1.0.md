# StarshipOS — The HONEST Operating Kernel System #
Base Specification v1.0
## 1. Identity ##

> ### "StarshipOS is an operating kernel system defined by epistemic honesty, not by language, userland, or compatibility promises."

### It is not:

- a FORTH OS
- a POSIX system
- a Linux clone
- a language-defined kernel

>### The kernel hosts virtual machines. One such VM may be StarForth; no VM defines the OS. ###
## 2. Immutability Rule ##
>### This Base Specification is immutable. It may change only by Amendment. All Amendments are immutable and may change only by further Amendment. Implementation behavior SHALL NOT redefine the specification.

## 3. Semantic Parity Invariant ##
>### Hosted execution and native (bare-metal) execution MUST be semantically identical. Any observable divergence is a spec violation unless authorized by Amendment. Hosted execution is the normative semantic reference. ###

## 4. Observability Axiom ##

>### Without execution and observation, no error state can exist. Observability is continuous: 0.00 < Observability < 1.00 Truth exists only within partial observability. Claims at either boundary are illegal.

## 5. Truth Model ##

>### All observable behavior exists in exactly one semantic state: ###

- Truth — defined by the Base Spec or Amendment
- Undefined — not yet defined; carries no guarantees
- Lie — contradicts defined Truth

>### Undefined is NEVER an error.

## 6. Illegal States (Non-Kernel) ##

There are exactly two non-kernel illegal states:

- Truth-Bounded Illegal State (Overpromise) Asserting Undefined behavior as Truth.
- Lie-Bounded Illegal State (Contradiction) Accepting behavior that contradicts defined Truth.

>### ILLEGAL_STATE exists only at the bookends of observability.

## 7. Configuration Invariant ##

>### The system exposes exactly two configuration knobs:

- Heartbeat (HB) — observation cadence
- Rolling Window of Truth (RWoT) — temporal observation aperture

>### Behavior under default values is defined. Behavior under non-default values is Undefined unless defined by Amendment. ###

## 8. Honesty Invariant ##

>### StarshipOS SHALL: ###

- adapt observation over time
- forbid false certainty
- forbid false denial
- forbid silent semantic drift