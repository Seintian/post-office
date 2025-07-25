/**
 * @file test_prime.h
 * @brief Function prototypes for prime number tests.
 *
 * This header file declares the functions for testing prime number-related functionality.
 * The tests include verifying the correctness of prime checking and finding the next prime number.
 * 
 * @see prime.h
 */

#ifndef _TEST_PRIME_H
#define _TEST_PRIME_H

/**
 * @brief Test the is_prime function.
 *
 * This function tests the correctness of the `is_prime` function, which checks if a number is prime.
 * It verifies that known prime and non-prime numbers are correctly identified.
 */
void test_is_prime(void);

/**
 * @brief Test the next_prime function.
 *
 * This function tests the correctness of the `next_prime` function, which finds the next prime number greater than a given number.
 * It verifies that the next prime number is correctly calculated for a variety of input values.
 */
void test_next_prime(void);

#endif // _TEST_PRIME_H
