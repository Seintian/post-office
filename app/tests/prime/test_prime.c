#include "prime/prime.h"
#include "unity/unity_fixture.h"
#include <stddef.h>
#include <stdint.h>
#include <stdlib.h>
#include <time.h>

TEST_GROUP(PRIME);

TEST_SETUP(PRIME) {}

TEST_TEAR_DOWN(PRIME) {}

// Reference implementation for validation (simple and independent)
static int ref_is_prime(size_t n) {
	if (n <= 1)
		return 0;
	if (n <= 3)
		return 1;
	if ((n % 2) == 0 || (n % 3) == 0)
		return n == 2 || n == 3; // handle 2,3 as prime
	// Trial division up to sqrt(n)
	for (size_t i = 5; i * i > 0 && i * i <= n; i += 6) { // guard overflow
		if ((n % i) == 0 || (n % (i + 2)) == 0)
			return 0;
	}
	return 1;
}

static size_t ref_next_prime(size_t n) {
	if (n <= 1)
		return 2;
	size_t k = n;
	for (;;) {
		k++;
		if (ref_is_prime(k))
			return k;
	}
}

TEST(PRIME, IsPrime_KnownSmall) {
	const size_t primes[] = {2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31, 97, 101, 103};
	for (size_t i = 0; i < sizeof(primes) / sizeof(primes[0]); i++) {
		TEST_ASSERT_TRUE(is_prime(primes[i]));
	}
}

TEST(PRIME, IsPrime_KnownComposites) {
	const size_t comps[] = {0, 1, 4, 6, 8, 9, 10, 12, 14, 15, 16, 18, 20, 21, 25, 27, 49, 100, 121, 143};
	for (size_t i = 0; i < sizeof(comps) / sizeof(comps[0]); i++) {
		TEST_ASSERT_FALSE(is_prime(comps[i]));
	}
}

TEST(PRIME, IsPrime_StructuralCases) {
	// Even numbers > 2 are not prime
	for (size_t e = 4; e <= 200; e += 2) {
		TEST_ASSERT_FALSE(is_prime(e));
	}
	// Squares of primes are not prime
	const size_t psmall[] = {2, 3, 5, 7, 11, 13, 17};
	for (size_t i = 0; i < sizeof(psmall) / sizeof(psmall[0]); i++) {
		size_t sq = psmall[i] * psmall[i];
		TEST_ASSERT_FALSE(is_prime(sq));
	}
}

TEST(PRIME, NextPrime_KnownPairs) {
	struct { size_t n, next; } cases[] = {
		{0, 2},  {1, 2},  {2, 3},   {3, 5},   {4, 5},   {14, 17}, {17, 19},
		{18, 19}, {19, 23}, {20, 23}, {100, 101}, {101, 103}, {102, 103},
		{103, 107}, {104, 107}, {1000, 1009}, {1024, 1031}, {4096, 4099},
		{7919, 7927}
	};
	for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); i++) {
		TEST_ASSERT_EQUAL_UINT64((unsigned long long)cases[i].next,
								 (unsigned long long)next_prime(cases[i].n));
	}
}

TEST(PRIME, NextPrime_BasicProperties) {
	for (size_t n = 0; n <= 2000; n++) {
		size_t p = next_prime(n);
		TEST_ASSERT_TRUE_MESSAGE(p > n, "next_prime(n) must be > n");
		TEST_ASSERT_TRUE(is_prime(p));
	}
}

TEST(PRIME, IsPrime_RandomAgainstReference) {
	// Keep within a practical bound for runtime
	srand(0xC0FFEE);
	const size_t samples = 2000;
	for (size_t i = 0; i < samples; i++) {
		unsigned int r = (unsigned int)rand();
		size_t n = (size_t)(r % 1000000u); // [0, 1e6)
		int ref = ref_is_prime(n);
		int got = is_prime(n) ? 1 : 0;
		char msg[96];
		snprintf(msg, sizeof msg, "Mismatch on n=%zu", n);
		TEST_ASSERT_EQUAL_INT_MESSAGE(ref, got, msg);
	}
}

TEST(PRIME, NextPrime_RandomAgainstReference) {
	srand(0xBADA55);
	const size_t samples = 300;
	for (size_t i = 0; i < samples; i++) {
		unsigned int r = (unsigned int)rand();
		size_t n = (size_t)(r % 1000000u);
		size_t refp = ref_next_prime(n);
		size_t gotp = next_prime(n);
		char msg[128];
		snprintf(msg, sizeof msg, "Mismatch next_prime at n=%zu: got=%zu ref=%zu", n, gotp, refp);
		TEST_ASSERT_EQUAL_UINT64_MESSAGE((unsigned long long)refp, (unsigned long long)gotp, msg);
	}
}

TEST(PRIME, NextPrime_GapHasNoPrimes) {
	srand(1234);
	const size_t samples = 200;
	for (size_t i = 0; i < samples; i++) {
		unsigned int r = (unsigned int)rand();
		size_t n = (size_t)(r % 1000000u);
		size_t p = next_prime(n);
		// Verify no prime strictly between n and p
		for (size_t k = n + 1; k < p; k++) {
			TEST_ASSERT_FALSE_MESSAGE(is_prime(k), "Found a prime in the gap before next_prime");
		}
	}
}

TEST_GROUP_RUNNER(PRIME) {
	RUN_TEST_CASE(PRIME, IsPrime_KnownSmall);
	RUN_TEST_CASE(PRIME, IsPrime_KnownComposites);
	RUN_TEST_CASE(PRIME, IsPrime_StructuralCases);
	RUN_TEST_CASE(PRIME, NextPrime_KnownPairs);
	RUN_TEST_CASE(PRIME, NextPrime_BasicProperties);
	RUN_TEST_CASE(PRIME, IsPrime_RandomAgainstReference);
	RUN_TEST_CASE(PRIME, NextPrime_RandomAgainstReference);
	RUN_TEST_CASE(PRIME, NextPrime_GapHasNoPrimes);
}

