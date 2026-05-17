# LithosAnanke System Architecture

**Version**: 0.1 (Draft)
**Date**: 2026-01-27
**Status**: Architectural Design

---

## 1. EXECUTIVE SUMMARY

LithosAnanke is a microkernel-style operating system where all system services run as isolated Virtual Machines (VMs) communicating exclusively through message passing. User identity is tied to physical media (thumbdrives), with cryptographically signed capability grants enforced at VM birth.

**Core Principles:**
- Everything is a VM, everything is a capsule
- All communication through messaging backbone (no exceptions)
- All storage through block I/O VM (no exceptions)
- Physical possession = authentication
- Cryptographic signing = capability enforcement
- One admin key, no recovery, no backdoors

---

## 2. VM HIERARCHY

### 2.1 Overview

```
┌─────────────────────────────────────────────────────────────────┐
│                         Mama VM                                 │
│                    (births and deaths ONLY)                     │
└─────────────────────────────────────────────────────────────────┘
                              │
            ┌─────────────────┼─────────────────┐
            │                 │                 │
            ▼                 ▼                 ▼
┌───────────────────┐ ┌───────────────┐ ┌───────────────────┐
│   Messaging VM    │ │  Block I/O VM │ │   User Session    │
│   (all comms)     │ │  (all storage)│ │      VMs...       │
└───────────────────┘ └───────────────┘ └───────────────────┘
```

### 2.2 Mama VM

**Role:** Root VM, births and deaths other VMs. Nothing else.

**Responsibilities:**
- Execute Mama init capsule at boot
- Birth system VMs (Messaging, Block I/O)
- Birth user session VMs on thumbdrive insertion
- Death user session VMs on thumbdrive removal
- Maintain VM registry

**Does NOT do:**
- Message routing (Messaging VM does this)
- Block I/O (Block I/O VM does this)
- User interaction (User VMs do this)

**Capsule:** `mama-init.4th` with flag `CAPSULE_FLAG_MAMA_INIT`

### 2.3 Messaging Backbone VM

**Role:** All inter-VM communication routes through this VM. No exceptions.

**Responsibilities:**
- Route messages between VMs
- Enforce message-level ACLs
- Queue messages for sleeping/busy VMs
- Handle hardware interrupts (keyboard, USB events)
- Notify Mama of hardware events (thumbdrive insertion)

**Capabilities Required:**
- `CAP_ROUTE_MESSAGES` - Can route between any VMs
- `CAP_HARDWARE_IRQ` - Can receive hardware interrupts
- `CAP_VM_ENUMERATE` - Can query VM registry

**Capsule:** `messaging.4th` with flag `CAPSULE_FLAG_PRODUCTION`

### 2.4 Block I/O VM

**Role:** All storage access routes through this VM. No exceptions.

**Responsibilities:**
- Physical block I/O to all storage devices
- Map user virtual blocks to physical blocks
- Enforce block-level ACLs
- Manage Block Allocation Maps (BAM)
- Handle storage device hotplug

**Capabilities Required:**
- `CAP_BLOCK_PHYSICAL` - Direct hardware access to storage
- `CAP_BLOCK_MAP` - Can create/modify block mappings
- `CAP_ACL_ENFORCE` - Can check ACLs on block operations

**Capsule:** `block-io.4th` with flag `CAPSULE_FLAG_PRODUCTION`

### 2.5 User Session VMs

**Role:** Isolated user environment with REPL.

**Created when:** User inserts authenticated thumbdrive

**Destroyed when:** Thumbdrive removed or user executes BYE

**Each user session gets:**
- Own VM instance
- Own heap (memory arena)
- Own RAM blocks (0-2047)
- Own disk blocks mapped to thumbdrive (2048+)
- Capabilities as granted by signed capability block

**Capsule:** `user-shell.4th` or user-specified custom capsule

---

## 3. BLOCK SUBSYSTEM ARCHITECTURE

### 3.1 User's Logical View (Deterministic)

Every user sees the identical logical layout:

```
┌─────────────────────────────────────────────────────────────┐
│ Blocks 0-2047: RAM Drive                                    │
│   - Fast, volatile working space                            │
│   - 2 MB total (2048 × 1024 bytes)                         │
│   - Cleared on session end                                  │
├─────────────────────────────────────────────────────────────┤
│ Blocks 2048+: Physical Storage (Thumbdrive)                 │
│   - Persistent across sessions                              │
│   - User's "home" blocks                                    │
│   - Block 2048 typically contains startup script            │
└─────────────────────────────────────────────────────────────┘
```

**Guarantees:**
- Blocks 0-2047 are ALWAYS RAM (fast, volatile)
- Blocks 2048+ are ALWAYS physical storage (persistent)
- This layout is identical for every user on every machine
- User FORTH code is portable (same logical addresses everywhere)

### 3.2 Physical Storage Layout (Thumbdrive)

```
┌─────────────────────────────────────────────────────────────┐
│ Devblock 0: Volume Header (4 KiB)                           │
│   - Magic, version, geometry                                │
│   - BAM location pointers                                   │
│   - Reserved range definitions                              │
├─────────────────────────────────────────────────────────────┤
│ Devblocks 1..B: Physical BAM                                │
│   - 1 bit per 1 KiB block                                   │
│   - Tracks allocated/free status                            │
├─────────────────────────────────────────────────────────────┤
│ Devblocks B+1..X: System Region                             │
│   - NOT VISIBLE to user (no logical block maps here)        │
│   - Identity sector (fingerprint source data)               │
│   - Signed capability grants                                │
│   - ACL tables (variable size, chained blocks)              │
│   - Audit log (grows over time)                             │
│   - Startup block pointer                                   │
├─────────────────────────────────────────────────────────────┤
│ Devblocks X+1..end: User Block Region                       │
│   - Mapped to user's logical blocks 2048+                   │
│   - User's FORTH code, data, definitions                    │
│   - Fully under user control (within ACL limits)            │
└─────────────────────────────────────────────────────────────┘
```

### 3.3 Virtual BAM

Each user session has a **Virtual BAM** that maps only their accessible blocks:

```
User's Virtual BAM                Physical Reality
─────────────────                 ─────────────────
Block 0     ─────────────────►    RAM buffer[0]
Block 1     ─────────────────►    RAM buffer[1]
...
Block 2047  ─────────────────►    RAM buffer[2047]
Block 2048  ─────────────────►    Thumbdrive devblock X+1
Block 2049  ─────────────────►    Thumbdrive devblock X+2
...
```

**Key property:** System region devblocks have NO entry in the virtual BAM. From the user's FORTH perspective, they don't exist. No block number maps to them.

### 3.4 Block I/O Flow

User VM requests a block read:

```
User VM                 Messaging VM           Block I/O VM
   │                         │                      │
   │ ─── MSG(READ,2048) ───► │                      │
   │                         │ ─── MSG(READ) ─────► │
   │                         │                      │
   │                         │                      ├─ Check ACL
   │                         │                      ├─ Map virtual→physical
   │                         │                      ├─ Physical I/O
   │                         │                      │
   │                         │ ◄─── MSG(DATA) ───── │
   │ ◄─── MSG(DATA) ──────── │                      │
   │                         │                      │
```

**No direct hardware access** from user VMs. All I/O is mediated.

### 3.5 System Region Uses

The system region (invisible to user) stores variable-size metadata:

| Data | Size | Notes |
|------|------|-------|
| Identity sector | ~256 bytes | Source data for fingerprint |
| Fingerprint | 32-64 bytes | SHA-256 or SHA-512 hash |
| Capability grants | Variable | Signed by admin |
| ACL tables | Variable | Block-level permissions |
| Group memberships | Variable | For shared access |
| Audit log | Grows | Tracks significant events |
| Startup pointer | 8 bytes | Which block to LOAD at init |
| Custom metadata | Variable | Application-specific |

**Variable-size data** uses block chaining (existing `prev_block`, `next_block` fields in `blk_meta_t`).

---

## 4. USER IDENTITY MODEL

### 4.1 Physical Capability Model

**Identity IS the thumbdrive.** No passwords, no usernames.

```
┌─────────────────────────────────────────────────────────────┐
│                    User's Thumbdrive                        │
├─────────────────────────────────────────────────────────────┤
│ Contains:                                                   │
│   - Identity data → SHA fingerprint                         │
│   - Signed capability grants                                │
│   - User's blocks (code, data)                             │
│   - Startup configuration                                   │
├─────────────────────────────────────────────────────────────┤
│ Properties:                                                 │
│   - Fully portable (works on any LithosAnanke system)      │
│   - Self-contained (no central user database)              │
│   - Physical possession = authentication                    │
└─────────────────────────────────────────────────────────────┘
```

### 4.2 Fingerprint Generation

The SHA fingerprint uniquely identifies the user:

```
Fingerprint = SHA-256(identity_sector_data)
```

**Identity sector contents** (stored in system region):
- UUID (random, generated at user creation)
- Creation timestamp
- Optional: hardware serial numbers, user metadata

**The fingerprint:**
- Is computed by the kernel at thumbdrive insertion
- Is matched against the signed capability grant
- Cannot be forged (would need admin signature)

### 4.3 Session Lifecycle

```
┌─────────────────────────────────────────────────────────────┐
│ 1. THUMBDRIVE INSERTION                                     │
├─────────────────────────────────────────────────────────────┤
│ • Hardware interrupt → Messaging VM                         │
│ • Messaging VM notifies Mama                                │
│ • Block I/O VM reads system region                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 2. IDENTITY VERIFICATION                                    │
├─────────────────────────────────────────────────────────────┤
│ • Compute SHA fingerprint from identity sector              │
│ • Read signed capability grant from system region           │
│ • Verify signature against fused admin public key           │
│ • If invalid: reject, log attempt, stop                     │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 3. VM BIRTH                                                 │
├─────────────────────────────────────────────────────────────┤
│ • Mama selects base capsule (user-shell or custom)          │
│ • Mama births new VM                                        │
│ • VM allocated: heap, RAM blocks (0-2047)                   │
│ • Disk blocks (2048+) mapped to thumbdrive                  │
│ • Capabilities granted: capsule ∩ signed_grant              │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 4. INITIALIZATION                                           │
├─────────────────────────────────────────────────────────────┤
│ • Base capsule executes (defines PERSONALITY)               │
│ • User's startup block LOADed (e.g., 2048 LOAD)            │
│ • Custom words, aliases, preferences applied                │
│ • REPL ready for user input                                 │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 5. ACTIVE SESSION                                           │
├─────────────────────────────────────────────────────────────┤
│ • User interacts via REPL                                   │
│ • All I/O through Block I/O VM                             │
│ • All comms through Messaging VM                            │
│ • ACLs enforced on every operation                          │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ 6. SESSION END (thumbdrive removal OR BYE)                  │
├─────────────────────────────────────────────────────────────┤
│ • Hardware interrupt (removal) or explicit BYE              │
│ • Messaging VM notifies Mama                                │
│ • Mama deaths the user VM                                   │
│ • RAM blocks freed                                          │
│ • Heap freed                                                │
│ • Block mappings removed                                    │
│ • Session over, no trace remains                            │
└─────────────────────────────────────────────────────────────┘
```

### 4.4 User Startup Customization

Users can customize their environment like `.bashrc`:

```forth
\ Block 2048 - User's startup script (example)

\ Load my personal library
2049 LOAD

\ Define aliases
: LS   DIR ;
: PWD  BLK@ . ;

\ Set preferences
DECIMAL
64 BASE-DEFAULT !

\ Greeting
." Welcome back, Captain!" CR
```

**Constraints:**
- Startup runs AFTER base capsule
- Can define words, load blocks, set variables
- CANNOT exceed capability grants
- CANNOT access blocks outside mapped range
- CANNOT send unauthorized messages

---

## 5. SECURITY MODEL

### 5.1 Root of Trust: Admin Fusing

**One-time, irreversible provisioning:**

```
┌─────────────────────────────────────────────────────────────┐
│              PROVISIONING MODE (First Boot Only)            │
├─────────────────────────────────────────────────────────────┤
│ 1. System detects unprovisioned state                       │
│ 2. Generate Ed25519 keypair                                 │
│    • Private key: 32 bytes                                  │
│    • Public key: 32 bytes                                   │
│ 3. FUSE public key to system                                │
│    • Written to NVRAM/secure storage                        │
│    • Cannot be changed after this                           │
│ 4. Create admin thumbdrive                                  │
│    • Private key stored in system region                    │
│    • Root capability grant (all permissions)                │
│    • Self-signed (bootstrap)                                │
│ 5. Exit provisioning mode FOREVER                           │
│    • Provisioning flag set                                  │
│    • Cannot re-enter this mode                              │
└─────────────────────────────────────────────────────────────┘
```

**After fusing:**
- Public key is immutable (fused to system)
- Private key exists ONLY on admin thumbdrive
- No copies, no backups, no recovery mechanism
- Lose admin thumbdrive = cannot provision new users

### 5.2 Admin Thumbdrive

**The single point of administrative authority:**

```
Admin Thumbdrive (System Region)
├── Admin identity sector
├── Root capability grant (self-signed at provisioning)
├── Private signing key (Ed25519)
└── Admin blocks (tools, scripts)

Admin Capabilities:
├── CAP_SIGN_GRANTS      - Can sign capability grants
├── CAP_CREATE_USERS     - Can provision new thumbdrives
├── CAP_REVOKE_USERS     - Can revoke capabilities
├── CAP_SYSTEM_CONFIG    - Can modify system settings
├── CAP_ALL_BLOCKS       - Can access any block
└── CAP_*                - All capabilities
```

**Admin can:**
- Create new user thumbdrives
- Sign capability grants
- Revoke user capabilities (by not re-signing)
- Access any user's blocks (for recovery, audit)
- Modify system configuration

**Admin CANNOT:**
- Create another admin (no delegation)
- Recover lost admin thumbdrive
- Change the fused public key

### 5.3 Capability Grant Structure

```
┌─────────────────────────────────────────────────────────────┐
│                   Capability Grant Block                    │
├─────────────────────────────────────────────────────────────┤
│ Header:                                                     │
│   Magic: "CPGR" (0x43504752)                               │
│   Version: 1                                                │
│   Grant ID: unique identifier                               │
│   Created: timestamp                                        │
│   Expires: timestamp (0 = never)                           │
├─────────────────────────────────────────────────────────────┤
│ Identity:                                                   │
│   Fingerprint: SHA-256 (32 bytes)                          │
│   Must match computed fingerprint at auth time              │
├─────────────────────────────────────────────────────────────┤
│ Capabilities (bitmap + parameters):                         │
│   CAP_BLOCK_READ        - Can read blocks                   │
│   CAP_BLOCK_WRITE       - Can write blocks                  │
│   CAP_BLOCK_RANGE       - Allowed block range (start, end)  │
│   CAP_MSG_SEND          - Can send messages                 │
│   CAP_MSG_RECEIVE       - Can receive messages              │
│   CAP_MSG_TARGETS       - Allowed message targets (VM list) │
│   CAP_CAPSULE_LOAD      - Can request capsule loading       │
│   CAP_CUSTOM_CAPSULE    - Can use custom init capsule       │
│   CAP_GROUP_MEMBER      - Group membership list             │
│   CAP_RESOURCE_QUOTA    - CPU, memory, block limits         │
├─────────────────────────────────────────────────────────────┤
│ Signature:                                                  │
│   Ed25519 signature over all above fields                   │
│   Signed by admin private key                               │
│   64 bytes                                                  │
└─────────────────────────────────────────────────────────────┘
```

### 5.4 Capability Verification Flow

```
Thumbdrive inserted
        │
        ▼
┌───────────────────────────────────────┐
│ 1. Read identity sector               │
│ 2. Compute fingerprint = SHA-256(id)  │
└───────────────────────────────────────┘
        │
        ▼
┌───────────────────────────────────────┐
│ 3. Read capability grant block        │
│ 4. Extract grant.fingerprint          │
│ 5. Compare: computed == grant.fp?     │
│    NO → Reject (identity mismatch)    │
└───────────────────────────────────────┘
        │ YES
        ▼
┌───────────────────────────────────────┐
│ 6. Extract grant.signature            │
│ 7. Verify: Ed25519_verify(            │
│       fused_public_key,               │
│       grant_data,                     │
│       signature)                      │
│    FAIL → Reject (tampered/forged)    │
└───────────────────────────────────────┘
        │ PASS
        ▼
┌───────────────────────────────────────┐
│ 8. Check expiry timestamp             │
│    EXPIRED → Reject                   │
└───────────────────────────────────────┘
        │ VALID
        ▼
┌───────────────────────────────────────┐
│ 9. Grant capabilities to new VM       │
│    effective_caps = grant.caps        │
└───────────────────────────────────────┘
```

### 5.5 Capability Enforcement Points

**At VM Birth:**
- Capabilities extracted from signed grant
- Stored in VM registry entry
- Cannot be modified after birth

**At Message Send:**
- Messaging VM checks: Does sender have CAP_MSG_SEND?
- Messaging VM checks: Is target in CAP_MSG_TARGETS?
- Reject if not authorized

**At Block Access:**
- Block I/O VM checks: Does requestor have CAP_BLOCK_READ/WRITE?
- Block I/O VM checks: Is block in CAP_BLOCK_RANGE?
- Reject if not authorized

**At Capsule Load:**
- Mama checks: Does requestor have CAP_CAPSULE_LOAD?
- Mama checks: Does requestor have CAP_CUSTOM_CAPSULE (if custom)?
- Reject if not authorized

### 5.6 Effective Capabilities

Final capabilities are the intersection:

```
effective_caps = capsule.declared ∩ grant.allowed ∩ system.policy
```

**capsule.declared:** What the init capsule requests
**grant.allowed:** What the signed grant permits
**system.policy:** System-wide limits (optional)

A user cannot gain capabilities not in their signed grant, even if the capsule requests them.

### 5.7 Revocation

**Capability revocation works by:**
1. Admin creates new grant with reduced capabilities
2. Admin re-signs the grant
3. New grant replaces old on thumbdrive
4. Next session uses new (reduced) capabilities

**Immediate revocation:**
- Admin can maintain a revocation list
- Checked at session start
- Fingerprint on revocation list = reject

**No online revocation checking** - this is a standalone system. Revocation requires physical access to update the thumbdrive or revocation list.

### 5.8 Attack Surface Analysis

**Attacks PREVENTED:**

| Attack | Prevention |
|--------|------------|
| Password guessing | No passwords exist |
| Credential theft | Must steal physical device |
| Privilege escalation | Capabilities signed, immutable |
| Block range escape | Virtual BAM has no mapping |
| Direct hardware access | All I/O through service VMs |
| Message spoofing | Source VM verified by Messaging VM |
| Capability forging | Ed25519 signature required |
| Admin impersonation | Admin private key on single device |
| Recovery mode exploit | No recovery mode exists |
| Backup compromise | No backups exist |

**Attacks NOT PREVENTED:**

| Attack | Notes |
|--------|-------|
| Physical theft of thumbdrive | Physical security is user's responsibility |
| Physical theft of admin drive | Total compromise, must reflash |
| Cold boot attack | RAM contents may be recoverable |
| Evil maid (hardware tampering) | Outside scope, requires secure boot |
| Side channel timing | Not addressed in current design |

### 5.9 Security Boundaries

```
┌─────────────────────────────────────────────────────────────┐
│                     KERNEL SPACE                            │
│  • Mama VM                                                  │
│  • VM scheduler                                             │
│  • Fused public key                                         │
│  • Hardware abstraction                                     │
└─────────────────────────────────────────────────────────────┘
        │
        │ Capsule-defined boundary
        ▼
┌─────────────────────────────────────────────────────────────┐
│                 PRIVILEGED VM SPACE                         │
│  • Messaging VM (CAP_ROUTE_MESSAGES)                        │
│  • Block I/O VM (CAP_BLOCK_PHYSICAL)                        │
└─────────────────────────────────────────────────────────────┘
        │
        │ Capability-checked boundary
        ▼
┌─────────────────────────────────────────────────────────────┐
│                    USER VM SPACE                            │
│  • User session VMs                                         │
│  • Constrained by signed capability grants                  │
│  • Cannot access other user's blocks                        │
│  • Cannot send unauthorized messages                        │
└─────────────────────────────────────────────────────────────┘
```

---

## 6. MESSAGING BACKBONE

### 6.1 Message Structure

```
┌─────────────────────────────────────────────────────────────┐
│                      Message Header                         │
├─────────────────────────────────────────────────────────────┤
│ msg_id:        uint64    Unique message identifier          │
│ source_vm:     uint32    Sender VM ID                       │
│ target_vm:     uint32    Recipient VM ID                    │
│ msg_type:      uint16    Message type code                  │
│ flags:         uint16    Delivery flags                     │
│ payload_len:   uint32    Payload size in bytes              │
│ timestamp:     uint64    Send timestamp (monotonic)         │
├─────────────────────────────────────────────────────────────┤
│                      Message Payload                        │
│ (variable length, message-type specific)                    │
└─────────────────────────────────────────────────────────────┘
```

### 6.2 Message Types

**System Messages:**
- `MSG_VM_BIRTH` - Mama→Messaging: new VM created
- `MSG_VM_DEATH` - Mama→Messaging: VM terminated
- `MSG_HARDWARE_EVENT` - Messaging→Mama: USB insert/remove

**Block I/O Messages:**
- `MSG_BLOCK_READ` - Request block read
- `MSG_BLOCK_WRITE` - Request block write
- `MSG_BLOCK_DATA` - Block data response
- `MSG_BLOCK_ACK` - Write acknowledgment
- `MSG_BLOCK_ERROR` - I/O error response

**User Messages:**
- `MSG_USER_DEFINED` - Application-specific messages

### 6.3 Message Routing

```
┌─────────────────────────────────────────────────────────────┐
│                    Messaging VM                             │
├─────────────────────────────────────────────────────────────┤
│ Inbound Queue ──► │ Route │ ──► Per-VM Outbound Queues     │
│                   │       │     ├── Mama Queue             │
│                   │       │     ├── Block I/O Queue        │
│                   │       │     ├── User VM 1 Queue        │
│                   │       │     ├── User VM 2 Queue        │
│                   │       │     └── ...                    │
├─────────────────────────────────────────────────────────────┤
│ Routing Logic:                                              │
│ 1. Verify source VM exists and is alive                     │
│ 2. Check source has CAP_MSG_SEND                           │
│ 3. Check target is in source's CAP_MSG_TARGETS             │
│ 4. Enqueue to target's outbound queue                       │
│ 5. Wake target VM if sleeping                               │
└─────────────────────────────────────────────────────────────┘
```

### 6.4 Synchronous vs Asynchronous

**Asynchronous (default):**
- Sender posts message and continues
- Message queued for recipient
- Response comes as separate message

**Synchronous (CALL pattern):**
- Sender posts message and blocks
- Waits for response message
- Useful for RPC-style operations (e.g., block read)

---

## 7. BOOT SEQUENCE

### 7.1 Full Boot Flow

```
┌─────────────────────────────────────────────────────────────┐
│ PHASE 0: Hardware Initialization                            │
├─────────────────────────────────────────────────────────────┤
│ • UEFI boot                                                 │
│ • Memory map from firmware                                  │
│ • Jump to kernel entry                                      │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 1: Kernel Initialization                              │
├─────────────────────────────────────────────────────────────┤
│ • Console init (serial UART)                                │
│ • PMM init (physical memory manager)                        │
│ • VMM init (virtual memory manager)                         │
│ • Heap init (kernel malloc)                                 │
│ • IDT init (interrupt descriptor table)                     │
│ • APIC init (interrupt controller)                          │
│ • Timer init (APIC timer, TSC calibration)                  │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 2: Provisioning Check                                 │
├─────────────────────────────────────────────────────────────┤
│ • Check provisioning flag in NVRAM                          │
│ • If NOT provisioned: enter provisioning mode               │
│   └── Generate keypair, fuse, create admin drive, exit      │
│ • If provisioned: load fused public key, continue           │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 3: Mama VM Creation                                   │
├─────────────────────────────────────────────────────────────┤
│ • Allocate Mama heap                                        │
│ • Create Mama VM (VM ID 0)                                  │
│ • Load mama-init capsule                                    │
│ • Execute Mama init                                         │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 4: System VM Birth                                    │
├─────────────────────────────────────────────────────────────┤
│ • Mama births Messaging VM from messaging.4th capsule       │
│ • Mama births Block I/O VM from block-io.4th capsule        │
│ • System services now running                               │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 5: Device Enumeration                                 │
├─────────────────────────────────────────────────────────────┤
│ • USB controller initialization                             │
│ • Storage device discovery                                  │
│ • Messaging VM registers for hardware events                │
└─────────────────────────────────────────────────────────────┘
                              │
                              ▼
┌─────────────────────────────────────────────────────────────┐
│ PHASE 6: Ready for Users                                    │
├─────────────────────────────────────────────────────────────┤
│ • System idle, waiting for thumbdrive insertion             │
│ • Console shows: "Insert thumbdrive to login"               │
│ • Or admin thumbdrive for administration                    │
└─────────────────────────────────────────────────────────────┘
```

### 7.2 Capsule Boot Order

```
Capsule Directory (in kernel or on system storage)
├── mama-init.4th        ← Loaded first, by kernel
├── messaging.4th        ← Birthed by Mama, second
├── block-io.4th         ← Birthed by Mama, third
├── user-shell.4th       ← Birthed on user login
└── (other capsules)     ← As needed
```

---

## 8. IMPLEMENTATION PRIORITIES

Based on dependencies:

### 8.1 Critical Path

```
1. USB Driver (XHCI)
   └── Need to detect thumbdrives

2. Block I/O Kernel Port
   └── Need to read thumbdrive system region

3. SHA-256 in Kernel
   └── Need to compute fingerprints

4. Ed25519 in Kernel
   └── Need to verify signatures

5. Messaging Backbone VM
   └── Need for all inter-VM communication

6. Block I/O VM
   └── Need for all storage access

7. Capability Enforcement
   └── Need to check grants at every operation

8. User Session VM
   └── Finally can have interactive users
```

### 8.2 Estimated Complexity

| Component | LOC Estimate | Complexity |
|-----------|--------------|------------|
| XHCI driver | ~2500 | High |
| USB mass storage | ~800 | Medium |
| Block subsystem port | ~500 | Medium (reuse existing) |
| SHA-256 | ~400 | Low (well-known algorithm) |
| Ed25519 | ~600 | Medium (or use library) |
| Capability grant parser | ~300 | Low |
| Messaging VM | ~1000 | Medium |
| Block I/O VM | ~800 | Medium |
| Capability enforcement | ~500 | Medium |
| User session management | ~600 | Medium |
| Admin provisioning | ~400 | Medium |

**Total: ~8,400 LOC** for core security infrastructure

---

## 9. DATA STRUCTURES SUMMARY

### 9.1 VM Registry Entry (Extended)

```c
typedef struct {
    uint32_t vm_id;                   // Unique VM identifier
    uint32_t state;                   // VMState enum
    uint64_t birth_capsule_id;        // Which capsule birthed this
    uint64_t birth_timestamp_ns;      // When born
    uint64_t birth_dict_hash;         // Dictionary hash at birth

    // Security fields (new)
    uint8_t  fingerprint[32];         // SHA-256 of user identity
    uint64_t capability_bitmap;       // Granted capabilities
    uint32_t block_range_start;       // First allowed block
    uint32_t block_range_end;         // Last allowed block
    uint32_t msg_target_count;        // Number of allowed targets
    uint32_t msg_targets[16];         // Allowed message target VMs

    // Resource tracking
    uint64_t heap_base;               // VM's heap start
    uint64_t heap_size;               // VM's heap size
    uint64_t ram_blocks_base;         // RAM block allocation

    uint32_t flags;
    uint32_t reserved;
} VMRegistryEntry;
```

### 9.2 Capability Bitmap

```c
// Capability flags
#define CAP_BLOCK_READ        (1ULL << 0)   // Can read blocks
#define CAP_BLOCK_WRITE       (1ULL << 1)   // Can write blocks
#define CAP_MSG_SEND          (1ULL << 2)   // Can send messages
#define CAP_MSG_RECEIVE       (1ULL << 3)   // Can receive messages
#define CAP_CAPSULE_LOAD      (1ULL << 4)   // Can request capsule load
#define CAP_CUSTOM_CAPSULE    (1ULL << 5)   // Can use custom capsule
#define CAP_SIGN_GRANTS       (1ULL << 6)   // Can sign capability grants (admin)
#define CAP_CREATE_USERS      (1ULL << 7)   // Can provision users (admin)
#define CAP_REVOKE_USERS      (1ULL << 8)   // Can revoke capabilities (admin)
#define CAP_SYSTEM_CONFIG     (1ULL << 9)   // Can modify system config (admin)
#define CAP_ALL_BLOCKS        (1ULL << 10)  // Can access any block (admin)
#define CAP_HARDWARE_IRQ      (1ULL << 11)  // Can receive hardware interrupts
#define CAP_ROUTE_MESSAGES    (1ULL << 12)  // Can route messages (messaging VM)
#define CAP_BLOCK_PHYSICAL    (1ULL << 13)  // Direct hardware I/O (block VM)
#define CAP_VM_ENUMERATE      (1ULL << 14)  // Can query VM registry
#define CAP_BLOCK_MAP         (1ULL << 15)  // Can create block mappings
#define CAP_ACL_ENFORCE       (1ULL << 16)  // Can enforce ACLs

// Convenience
#define CAP_USER_DEFAULT      (CAP_BLOCK_READ | CAP_BLOCK_WRITE | \
                               CAP_MSG_SEND | CAP_MSG_RECEIVE)
#define CAP_ADMIN_ALL         (0xFFFFFFFFFFFFFFFFULL)
```

### 9.3 Signed Capability Grant (On-disk)

```c
typedef struct __attribute__((packed)) {
    // Header (16 bytes)
    uint32_t magic;                   // "CPGR" = 0x43504752
    uint16_t version;                 // Grant format version
    uint16_t flags;                   // Grant flags
    uint64_t grant_id;                // Unique grant identifier

    // Timestamps (16 bytes)
    uint64_t created_ns;              // Creation timestamp
    uint64_t expires_ns;              // Expiry (0 = never)

    // Identity (32 bytes)
    uint8_t  fingerprint[32];         // SHA-256 that must match

    // Capabilities (32 bytes)
    uint64_t capability_bitmap;       // Granted capabilities
    uint32_t block_range_start;       // First allowed block (logical)
    uint32_t block_range_end;         // Last allowed block (logical)
    uint32_t msg_target_count;        // Number of target VMs
    uint32_t msg_targets[4];          // Allowed targets (expandable)

    // Quotas (16 bytes)
    uint32_t max_heap_mb;             // Maximum heap size
    uint32_t max_ram_blocks;          // Maximum RAM blocks
    uint32_t max_disk_blocks;         // Maximum disk blocks
    uint32_t reserved;

    // Signature (64 bytes)
    uint8_t  signature[64];           // Ed25519 signature

} CapabilityGrant;  // Total: 176 bytes
```

---

## 10. GLOSSARY

| Term | Definition |
|------|------------|
| **Mama VM** | Root VM that births and deaths all other VMs |
| **Baby VM** | Any VM birthed by Mama |
| **Capsule** | Immutable init block sequence defining VM personality |
| **PERSONALITY** | VM's immutable identity after birth (from capsule) |
| **DOMAIN** | Mama's construction space (never visible to babies) |
| **Fingerprint** | SHA-256 hash of identity sector, identifies user |
| **Capability Grant** | Signed document specifying user's permissions |
| **Fused Key** | Admin public key permanently written at provisioning |
| **Virtual BAM** | Per-user block allocation map (subset view) |
| **System Region** | Thumbdrive area invisible to user (metadata, ACLs) |
| **Admin Thumbdrive** | Single device with signing authority |

---

## 11. OPEN QUESTIONS

1. **Multiple simultaneous users?** Can multiple thumbdrives be inserted at once, with multiple concurrent sessions?

2. **Shared blocks between users?** Can users share specific blocks with each other? (Would require group capabilities)

3. **Network authentication?** Future extension for remote login?

4. **Audit log rotation?** System region audit log grows forever - need rotation policy?

5. **Capability delegation?** Can a user grant subset of their capabilities to another? (Currently: no)

6. **Time-limited sessions?** Automatic logout after idle timeout?

---

## 12. REVISION HISTORY

| Version | Date | Changes |
|---------|------|---------|
| 0.1 | 2026-01-27 | Initial architecture draft |

---

**Document Status:** Draft - Pending Review

**Authors:** Claude (AI Assistant), Robert A. James

**Next Steps:**
1. Review and refine security model
2. Prototype USB detection
3. Implement Ed25519/SHA-256 in kernel
4. Design message format in detail
5. Implement Block I/O VM capsule