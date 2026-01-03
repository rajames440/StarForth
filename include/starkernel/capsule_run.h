/*
  StarForth — Steady-State Virtual Machine Runtime

  Copyright (c) 2023–2025 Robert A. James
  All rights reserved.

  This file is part of the StarForth project.

  Licensed under the StarForth License, Version 1.0 (the "License");
  you may not use this file except in compliance with the License.

  You may obtain a copy of the License at:
      https://github.com/star.4th@proton.me/StarForth/LICENSE.txt

  This software is provided "AS IS", WITHOUT WARRANTY OF ANY KIND,
  express or implied, including but not limited to the warranties of
  merchantability, fitness for a particular purpose, and noninfringement.

  See the License for the specific language governing permissions and
  limitations under the License.
*/

/**
 * capsule_run.h - DoE Run Logging (M7.1)
 *
 * Structures for logging capsule execution runs and VM births.
 * Supports provenance tracking and experiment reproducibility.
 */

#ifndef STARKERNEL_CAPSULE_RUN_H
#define STARKERNEL_CAPSULE_RUN_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/*===========================================================================
 * Run Log Configuration
 *===========================================================================*/

/** Phase A: Fixed ring buffer size */
#define CAPSULE_MAX_RUN_RECORDS     1024

/*===========================================================================
 * Result Codes
 *===========================================================================*/

typedef enum {
    CAPSULE_RUN_OK = 0,
    CAPSULE_RUN_ERR_INVALID,       /* Invalid capsule */
    CAPSULE_RUN_ERR_NOT_ELIGIBLE,  /* Capsule not eligible for operation */
    CAPSULE_RUN_ERR_EXEC_FAIL,     /* Execution failed */
    CAPSULE_RUN_ERR_HASH_MISMATCH, /* Post-run hash mismatch */
    CAPSULE_RUN_ERR_STILLBORN,     /* VM birth failed */
} CapsuleRunResult;

/*===========================================================================
 * CapsuleRunRecord - DoE Execution Log Entry
 *===========================================================================*/

typedef struct {
    uint64_t run_id;         /* Sequential run identifier */
    uint32_t vm_id;          /* Which VM executed this */
    uint32_t reserved;       /* Padding */
    uint64_t capsule_id;     /* Which capsule was run */
    uint64_t capsule_hash;   /* Hash at time of execution */
    uint64_t pre_dict_hash;  /* Dictionary state before run */
    uint64_t post_dict_hash; /* Dictionary state after run */
    uint64_t started_ns;     /* Monotonic start time */
    uint64_t ended_ns;       /* Monotonic end time */
    uint32_t result_code;    /* CapsuleRunResult */
    uint32_t flags;          /* Run flags (mode, etc.) */
} CapsuleRunRecord;

/*===========================================================================
 * VM Registry Entry
 *===========================================================================*/

typedef enum {
    VM_STATE_EMBRYO = 0,     /* Allocated but not yet born */
    VM_STATE_LIVE,           /* Successfully born, operational */
    VM_STATE_STILLBORN,      /* Birth failed */
    VM_STATE_DEAD,           /* Terminated */
} VMState;

typedef struct {
    uint32_t vm_id;              /* Assigned at birth, immutable */
    uint32_t state;              /* VMState */
    uint64_t birth_capsule_id;   /* Which capsule birthed this VM */
    uint64_t birth_timestamp_ns; /* When VM was born */
    uint64_t birth_dict_hash;    /* Dictionary hash after birth */
    uint32_t flags;              /* VM flags */
    uint32_t reserved;           /* Padding */
} VMRegistryEntry;

/*===========================================================================
 * Run Log Functions
 *===========================================================================*/

/**
 * capsule_run_log_init - Initialize run log
 */
void capsule_run_log_init(void);

/**
 * capsule_run_log_record - Log a run record
 *
 * @param record  Record to log
 * @return Run ID assigned, or 0 on failure
 */
uint64_t capsule_run_log_record(const CapsuleRunRecord *record);

/**
 * capsule_run_log_get - Get a run record by ID
 *
 * @param run_id  Run ID to retrieve
 * @param out     Output record
 * @return 0 on success, -1 if not found
 */
int capsule_run_log_get(uint64_t run_id, CapsuleRunRecord *out);

/**
 * capsule_run_log_count - Get number of logged runs
 */
uint32_t capsule_run_log_count(void);

/*===========================================================================
 * Parity Logging
 *===========================================================================*/

/**
 * capsule_parity_log_birth - Log VM birth parity record
 *
 * Output format:
 *   PARITY:BIRTH vm_id=N capsule_id=X mode=p capsule_hash=H dict_hash=D
 */
void capsule_parity_log_birth(
    uint32_t vm_id,
    uint64_t capsule_id,
    uint64_t capsule_hash,
    uint64_t dict_hash
);

/**
 * capsule_parity_log_birth_failed - Log failed birth
 *
 * Output format:
 *   PARITY:BIRTH_FAILED vm_id=N capsule_id=X error=E partial_dict_hash=H
 */
void capsule_parity_log_birth_failed(
    uint32_t vm_id,
    uint64_t capsule_id,
    CapsuleRunResult error,
    uint64_t partial_dict_hash
);

/**
 * capsule_parity_log_run - Log DoE run parity record
 *
 * Output format:
 *   PARITY:RUN vm_id=N run_id=R capsule_id=X mode=e pre_dict=P post_dict=Q
 */
void capsule_parity_log_run(
    uint32_t vm_id,
    uint64_t run_id,
    uint64_t capsule_id,
    uint64_t pre_dict_hash,
    uint64_t post_dict_hash
);

/**
 * capsule_parity_log_mama_init - Log Mama init parity record
 *
 * Output format:
 *   PARITY:MAMA_INIT capsule_id=X mode=m capsule_hash=H dict_hash=D
 */
void capsule_parity_log_mama_init(
    uint64_t capsule_id,
    uint64_t capsule_hash,
    uint64_t dict_hash
);

/**
 * capsule_parity_set_output - Set output hooks for parity logging
 *
 * @param putc_fn  Function to output a single character
 * @param puts_fn  Function to output a string
 */
void capsule_parity_set_output(
    void (*putc_fn)(char),
    void (*puts_fn)(const char *)
);

#ifdef __cplusplus
}
#endif

#endif /* STARKERNEL_CAPSULE_RUN_H */
