/**
 * @file doxygen_example.h
 * @brief Example header file showing Doxygen documentation style
 *
 * @details
 * This file demonstrates how to properly document C code using Doxygen/Javadoc
 * style comments for the StarForth project. Use this as a template when adding
 * documentation to existing headers.
 *
 * ## Documentation Goals
 * - Make API clear and easy to understand
 * - Provide usage examples for complex functions
 * - Document pre/post conditions and edge cases
 * - Cross-reference related functions
 *
 * ## Quick Reference
 * - File docs: `@file`, `@brief`, `@details`, `@author`
 * - Function docs: `@brief`, `@param`, `@return`, `@details`
 * - Examples: `@code ... @endcode`
 * - Cross-refs: `@see function_name`
 * - Warnings: `@warning`, `@note`, `@bug`, `@todo`
 *
 * @author R. A. James (rajames) - StarshipOS Forth Project
 * @date 2025-10-01
 * @version 1.0.0
 * @copyright CC0-1.0 (Public Domain)
 *
 * @see docs/DOXYGEN_STYLE_GUIDE.md
 */

#ifndef DOXYGEN_EXAMPLE_H
#define DOXYGEN_EXAMPLE_H

#include <stdint.h>
#include <stddef.h>

/*===========================================================================
 * TYPE DEFINITIONS
 *===========================================================================*/

/**
 * @typedef value_t
 * @brief Example signed integer value type
 *
 * Used for all numeric operations in this example. On 64-bit platforms,
 * this is a signed 64-bit integer.
 *
 * @note Platform-dependent size (sizeof(long))
 * @see unsigned_value_t
 */
typedef long value_t;

/**
 * @typedef unsigned_value_t
 * @brief Example unsigned integer value type
 *
 * Used for bit manipulation and unsigned arithmetic.
 *
 * @see value_t
 */
typedef unsigned long unsigned_value_t;

/**
 * @enum operation_mode_t
 * @brief Operating mode enumeration
 *
 * Defines the two primary modes of operation for the example system.
 */
typedef enum {
    MODE_NORMAL = 0, /**< Normal operation mode */
    MODE_DEBUG = 1, /**< Debug mode with verbose logging */
    MODE_FAST = 2 /**< Fast mode, skip safety checks */
} operation_mode_t;

/**
 * @struct example_context
 * @brief Example context structure
 *
 * @details
 * Contains all state needed for example operations. One context per
 * independent operation stream.
 *
 * ## Lifetime
 * 1. Allocate: `example_context *ctx = malloc(sizeof(example_context));`
 * 2. Initialize: `example_init(ctx);`
 * 3. Use: `example_process(ctx, data);`
 * 4. Cleanup: `example_cleanup(ctx); free(ctx);`
 *
 * ## Thread Safety
 * Not thread-safe. Use separate contexts per thread or external locking.
 *
 * @warning Always call example_init() before using context
 * @see example_init()
 * @see example_cleanup()
 */
typedef struct example_context {
    /**
     * @brief Current operating mode
     * @see operation_mode_t
     */
    operation_mode_t mode;

    /**
     * @brief Internal buffer for processing
     * @note Allocated by example_init(), freed by example_cleanup()
     */
    uint8_t *buffer;

    /**
     * @brief Size of buffer in bytes
     * @invariant buffer_size >= 1024
     */
    size_t buffer_size;

    /**
     * @brief Current position in buffer
     * @invariant 0 <= position < buffer_size
     */
    size_t position;

    /**
     * @brief Error flag (0=success, non-zero=error code)
     *
     * @details
     * Error codes:
     * - 0: No error
     * - 1: Out of memory
     * - 2: Invalid parameter
     * - 3: Buffer overflow
     * - 4: Processing error
     */
    int error;

    /**
     * @brief Number of operations performed
     * @note Wraps around at UINT64_MAX
     */
    uint64_t operation_count;
} example_context;

/*===========================================================================
 * MACROS AND CONSTANTS
 *===========================================================================*/

/**
 * @def EXAMPLE_VERSION_MAJOR
 * @brief Major version number
 */
#define EXAMPLE_VERSION_MAJOR 1

/**
 * @def EXAMPLE_VERSION_MINOR
 * @brief Minor version number
 */
#define EXAMPLE_VERSION_MINOR 0

/**
 * @def EXAMPLE_VERSION_PATCH
 * @brief Patch version number
 */
#define EXAMPLE_VERSION_PATCH 0

/**
 * @def EXAMPLE_BUFFER_SIZE
 * @brief Default buffer size in bytes (16 KB)
 *
 * Used when no explicit size is provided to example_init_sized().
 * Must be at least 1024 bytes.
 *
 * @see example_init_sized()
 */
#define EXAMPLE_BUFFER_SIZE (16 * 1024)

/**
 * @def EXAMPLE_MAX_VALUE
 * @brief Maximum allowed value for operations
 *
 * Values exceeding this limit will be clamped by example_clamp().
 */
#define EXAMPLE_MAX_VALUE 1000000

/*===========================================================================
 * FUNCTION DECLARATIONS
 *===========================================================================*/

/**
 * @defgroup init_group Initialization and Cleanup
 * @brief Context lifecycle management functions
 * @{
 */

/**
 * @brief Initialize example context with default settings
 *
 * @details
 * Allocates internal buffers and sets up the context for operation.
 * The context will be initialized with:
 * - Mode: MODE_NORMAL
 * - Buffer size: EXAMPLE_BUFFER_SIZE
 * - Position: 0
 * - Error: 0
 * - Operation count: 0
 *
 * @param ctx Pointer to uninitialized context
 *
 * @return 0 on success, non-zero error code on failure
 * @retval 0 Success
 * @retval 1 Memory allocation failed
 * @retval 2 Invalid ctx pointer (NULL)
 *
 * @pre ctx must be non-NULL
 * @post On success: ctx->buffer is allocated, ctx->error is 0
 * @post On failure: ctx->error contains error code, ctx->buffer is NULL
 *
 * @note Always check return value before using context
 * @warning Must call example_cleanup() when done to avoid memory leaks
 *
 * @see example_init_sized()
 * @see example_cleanup()
 *
 * @par Example:
 * @code
 * example_context ctx;
 * if (example_init(&ctx) != 0) {
 *     fprintf(stderr, "Initialization failed: error %d\n", ctx.error);
 *     return 1;
 * }
 * // Use context...
 * example_cleanup(&ctx);
 * @endcode
 */
int example_init(example_context *ctx);

/**
 * @brief Initialize example context with custom buffer size
 *
 * @details
 * Like example_init() but allows specifying a custom buffer size.
 * Useful for processing large data sets or operating in memory-constrained
 * environments.
 *
 * @param ctx Pointer to uninitialized context
 * @param buffer_size Desired buffer size in bytes
 *
 * @return 0 on success, non-zero error code on failure
 *
 * @pre ctx must be non-NULL
 * @pre buffer_size must be >= 1024
 * @post On success: ctx->buffer_size == buffer_size
 *
 * @note Buffer size will be rounded up to nearest multiple of 64 bytes
 * @warning Sizes > 10MB may cause performance issues
 *
 * @see example_init()
 */
int example_init_sized(example_context *ctx, size_t buffer_size);

/**
 * @brief Clean up and free resources associated with context
 *
 * @details
 * Frees the internal buffer and resets all fields to safe values.
 * After calling this function, the context must be re-initialized
 * before use.
 *
 * @param ctx Pointer to initialized context
 *
 * @pre ctx must have been successfully initialized
 * @post ctx->buffer is NULL, all fields reset
 *
 * @note Safe to call multiple times (idempotent)
 * @note Does NOT free the ctx structure itself (caller's responsibility)
 *
 * @see example_init()
 *
 * @par Example:
 * @code
 * example_context *ctx = malloc(sizeof(example_context));
 * example_init(ctx);
 * // ... use context ...
 * example_cleanup(ctx);  // Frees internal buffer
 * free(ctx);             // Free context itself
 * @endcode
 */
void example_cleanup(example_context *ctx);

/** @} */ // End of init_group

/**
 * @defgroup process_group Processing Functions
 * @brief Data processing and transformation operations
 * @{
 */

/**
 * @brief Process a value using the context
 *
 * @details
 * Performs the primary processing operation on the input value.
 * The exact operation depends on the context's current mode:
 *
 * - MODE_NORMAL: Standard processing with safety checks
 * - MODE_DEBUG: Slow processing with verbose logging
 * - MODE_FAST: Optimized processing, minimal checks
 *
 * This function is thread-safe if called on different contexts,
 * but NOT thread-safe if multiple threads use the same context.
 *
 * @param ctx Pointer to initialized context
 * @param value Input value to process
 *
 * @return Processed value, or 0 on error
 *
 * @pre ctx must be initialized and error-free
 * @pre value must be in range [-EXAMPLE_MAX_VALUE, EXAMPLE_MAX_VALUE]
 * @post ctx->operation_count is incremented
 * @post On error: ctx->error is set
 *
 * @note Out-of-range values are clamped, not rejected
 * @warning Check ctx->error after calling
 *
 * @see example_process_batch()
 * @see example_set_mode()
 *
 * @par Performance:
 * - MODE_NORMAL: ~100 ns/call
 * - MODE_DEBUG: ~500 ns/call
 * - MODE_FAST: ~50 ns/call
 *
 * @par Example:
 * @code
 * example_context ctx;
 * example_init(&ctx);
 * example_set_mode(&ctx, MODE_FAST);
 *
 * value_t result = example_process(&ctx, 42);
 * if (ctx.error) {
 *     fprintf(stderr, "Processing failed\n");
 * }
 * example_cleanup(&ctx);
 * @endcode
 */
value_t example_process(example_context *ctx, value_t value);

/**
 * @brief Process multiple values in batch
 *
 * @details
 * More efficient than calling example_process() in a loop for large
 * data sets. Uses SIMD optimizations when available.
 *
 * @param ctx Pointer to initialized context
 * @param input Array of input values
 * @param output Array to receive processed values
 * @param count Number of values to process
 *
 * @return Number of values successfully processed
 *
 * @pre ctx must be initialized
 * @pre input and output must be non-NULL
 * @pre count > 0
 * @pre input and output must not overlap
 * @post output[0..retval-1] contains processed values
 *
 * @note Partial processing is possible if error occurs mid-batch
 * @note Returns count on complete success
 *
 * @warning Large batches (> 1M values) may block for extended periods
 *
 * @see example_process()
 *
 * @par Example:
 * @code
 * value_t input[1000] = {1, 2, 3, ...};
 * value_t output[1000];
 *
 * size_t processed = example_process_batch(&ctx, input, output, 1000);
 * printf("Processed %zu of 1000 values\n", processed);
 * @endcode
 */
size_t example_process_batch(example_context *ctx, const value_t *input,
                             value_t *output, size_t count);

/** @} */ // End of process_group

/**
 * @defgroup config_group Configuration Functions
 * @brief Context configuration and status queries
 * @{
 */

/**
 * @brief Set operating mode
 *
 * @param ctx Pointer to initialized context
 * @param mode New mode to set
 *
 * @pre ctx must be initialized
 * @pre mode must be valid (MODE_NORMAL, MODE_DEBUG, or MODE_FAST)
 * @post ctx->mode == mode
 *
 * @note Mode changes take effect immediately
 * @note MODE_FAST disables some safety checks - use carefully
 *
 * @see operation_mode_t
 * @see example_get_mode()
 */
void example_set_mode(example_context *ctx, operation_mode_t mode);

/**
 * @brief Get current operating mode
 *
 * @param ctx Pointer to initialized context
 * @return Current mode
 *
 * @pre ctx must be initialized
 *
 * @see example_set_mode()
 */
operation_mode_t example_get_mode(const example_context *ctx);

/**
 * @brief Get operation count
 *
 * @param ctx Pointer to initialized context
 * @return Number of operations performed since initialization
 *
 * @pre ctx must be initialized
 *
 * @note Counter wraps at UINT64_MAX
 * @see example_context::operation_count
 */
uint64_t example_get_operation_count(const example_context *ctx);

/** @} */ // End of config_group

/**
 * @defgroup util_group Utility Functions
 * @brief Helper and utility functions
 * @{
 */

/**
 * @brief Clamp value to valid range
 *
 * @details
 * Ensures value is within [-EXAMPLE_MAX_VALUE, EXAMPLE_MAX_VALUE].
 * Values outside this range are adjusted to the nearest boundary.
 *
 * @param value Input value
 * @return Clamped value
 *
 * @post Return value is in range [-EXAMPLE_MAX_VALUE, EXAMPLE_MAX_VALUE]
 *
 * @note Pure function (no side effects)
 *
 * @par Example:
 * @code
 * value_t x = example_clamp(2000000);  // x = 1000000
 * value_t y = example_clamp(500);      // y = 500
 * value_t z = example_clamp(-2000000); // z = -1000000
 * @endcode
 */
value_t example_clamp(value_t value);

/**
 * @brief Get version string
 *
 * @return Static version string (e.g., "1.0.0")
 *
 * @note Return value is a static string - do not free()
 * @note Thread-safe
 *
 * @par Example:
 * @code
 * printf("Example library version: %s\n", example_get_version());
 * @endcode
 */
const char *example_get_version(void);

/**
 * @brief Check if value is valid
 *
 * @param value Value to check
 * @return 1 if valid, 0 if invalid
 *
 * @note Valid values are in range [-EXAMPLE_MAX_VALUE, EXAMPLE_MAX_VALUE]
 * @see example_clamp()
 */
int example_is_valid(value_t value);

/** @} */ // End of util_group

#endif /* DOXYGEN_EXAMPLE_H */