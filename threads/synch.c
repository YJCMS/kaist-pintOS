/* This file is derived from source code for the Nachos
   instructional operating system.  The Nachos copyright notice
   is reproduced in full below. */

/* Copyright (c) 1992-1996 The Regents of the University of California.
   All rights reserved.

   Permission to use, copy, modify, and distribute this software
   and its documentation for any purpose, without fee, and
   without written agreement is hereby granted, provided that the
   above copyright notice and the following two paragraphs appear
   in all copies of this software.

   IN NO EVENT SHALL THE UNIVERSITY OF CALIFORNIA BE LIABLE TO
   ANY PARTY FOR DIRECT, INDIRECT, SPECIAL, INCIDENTAL, OR
   CONSEQUENTIAL DAMAGES ARISING OUT OF THE USE OF THIS SOFTWARE
   AND ITS DOCUMENTATION, EVEN IF THE UNIVERSITY OF CALIFORNIA
   HAS BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

   THE UNIVERSITY OF CALIFORNIA SPECIFICALLY DISCLAIMS ANY
   WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
   WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
   PURPOSE.  THE SOFTWARE PROVIDED HEREUNDER IS ON AN "AS IS"
   BASIS, AND THE UNIVERSITY OF CALIFORNIA HAS NO OBLIGATION TO
   PROVIDE MAINTENANCE, SUPPORT, UPDATES, ENHANCEMENTS, OR
   MODIFICATIONS.
   */

#include "threads/synch.h"
#include <stdio.h>
#include <string.h>
#include "threads/interrupt.h"
#include "threads/thread.h"

void donate_priority(struct lock *lock);

/* Initializes semaphore SEMA to VALUE.  A semaphore is a
   nonnegative integer along with two atomic operators for
   manipulating it:

   - down or "P": wait for the value to become positive, then
   decrement it.

   - up or "V": increment the value (and wake up one waiting
   thread, if any). */
// 세마포어 구조체 sema를 value값으로 초기화
void
sema_init (struct semaphore *sema, unsigned value) {
	ASSERT (sema != NULL);

	sema->value = value; // 부호없는 정수형 저장
	list_init (&sema->waiters); // wait list 생성
}

/* Down or "P" operation on a semaphore.  Waits for SEMA's value
   to become positive and then atomically decrements it.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but if it sleeps then the next scheduled
   thread will probably turn interrupts back on. This is
   sema_down function. */
// “Down” or “P” 연산을 sema에 실행, sema값이 양수가 되면 1만큼 빼기

// 우선순위 정렬 함수 추가
bool
sema_priority_desc (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED) {
	const struct thread *a = list_entry (a_, struct thread, elem);
	const struct thread *b = list_entry (b_, struct thread, elem);
	return a->priority > b->priority; // 우선순위 높을 수록 우선순위이며 리스트 앞으로 배치
}

bool
lock_priority_desc (const struct list_elem *a_, const struct list_elem *b_, void *aux UNUSED) {
	const struct lock *a = list_entry (a_, struct lock, elem);
	const struct lock *b = list_entry (b_, struct lock, elem);
	return a->max_priority > b->max_priority; // 우선순위 높을 수록 우선순위이며 리스트 앞으로 배치
}

void
sema_down (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);
	ASSERT (!intr_context ());

	old_level = intr_disable ();
	while (sema->value == 0) {
		// 원래 코드
		// list_push_back (&sema->waiters, &thread_current ()->elem);

		// project1: 구현
		// sema waiter list가 priority가 큰 순서로 insert_ordered되도록 바꾸기
		list_insert_ordered(&sema->waiters, &thread_current()->elem, sema_priority_desc, NULL);
		thread_block ();
	}
	sema->value--;
	intr_set_level (old_level);
}

/* Down or "P" operation on a semaphore, but only if the
   semaphore is not already 0.  Returns true if the semaphore is
   decremented, false otherwise.

   This function may be called from an interrupt handler. */
// “Down” or “P” 연산을 기다리지않고 시도함, 성공적으로 감소하면 true, 0이었으면 false 리턴(비효율적)
bool
sema_try_down (struct semaphore *sema) {
	enum intr_level old_level;
	bool success;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	if (sema->value > 0)
	{
		sema->value--;
		success = true;
	}
	else
		success = false;
	intr_set_level (old_level);

	return success;
}

/* Up or "V" operation on a semaphore.  Increments SEMA's value
   and wakes up one thread of those waiting for SEMA, if any.

   This function may be called from an interrupt handler. */
//  “Up” or “V” sema값을 증가시키는 연산 실행, 만약에 sema가 기다리는 쓰레드가 있다면 깨우기
void
sema_up (struct semaphore *sema) {
	enum intr_level old_level;

	ASSERT (sema != NULL);

	old_level = intr_disable ();
	// sema 문제에서 첫번째 컨텍스트 스위칭이 안되는 이유 세마가 0이기 때문
	// sema->value++의 위치를 옮겨서 thread_unblock이 일어나기전에 1로 만들어줌
	sema->value++;
	if (!list_empty (&sema->waiters)) {
		thread_unblock (list_entry (list_pop_front (&sema->waiters), struct thread, elem));
		thread_check_yield();
	}
	// 원래 sema->value++; 위치
	intr_set_level (old_level);
}

static void sema_test_helper (void *sema_);

/* Self-test for semaphores that makes control "ping-pong"
   between a pair of threads.  Insert calls to printf() to see
   what's going on. */
void
sema_self_test (void) {
	struct semaphore sema[2];
	int i;

	printf ("Testing semaphores...");
	sema_init (&sema[0], 0);
	sema_init (&sema[1], 0);
	thread_create ("sema-test", PRI_DEFAULT, sema_test_helper, &sema);
	for (i = 0; i < 10; i++)
	{
		sema_up (&sema[0]);
		sema_down (&sema[1]);
	}
	printf ("done.\n");
}

/* Thread function used by sema_self_test(). */
static void
sema_test_helper (void *sema_) {
	struct semaphore *sema = sema_;
	int i;

	for (i = 0; i < 10; i++)
	{
		sema_down (&sema[0]);
		sema_up (&sema[1]);
	}
}

/* Initializes LOCK.  A lock can be held by at most a single
   thread at any given time.  Our locks are not "recursive", that
   is, it is an error for the thread currently holding a lock to
   try to acquire that lock.

   A lock is a specialization of a semaphore with an initial
   value of 1.  The difference between a lock and such a
   semaphore is twofold.  First, a semaphore can have a value
   greater than 1, but a lock can only be owned by a single
   thread at a time.  Second, a semaphore does not have an owner,
   meaning that one thread can "down" the semaphore and then
   another one "up" it, but with a lock the same thread must both
   acquire and release it.  When these restrictions prove
   onerous, it's a good sign that a semaphore should be used,
   instead of a lock. */
void
lock_init (struct lock *lock) {
	ASSERT (lock != NULL);

	lock->holder = NULL;
	sema_init (&lock->semaphore, 1);
	// project1 priority-donate-multi: 
	lock->max_priority = PRI_DNTD_INIT;
}

/* Acquires LOCK, sleeping until it becomes available if
   necessary.  The lock must not already be held by the current
   thread.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
lock_acquire (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (!lock_held_by_current_thread (lock));

	thread_current()->wanted_lock = lock;

	// 기부가 일어나고
	donate_priority(lock);
	
	sema_down (&lock->semaphore);
	
	// 락획득 성공하면 쓰레드가 가진 락 리스트에 넣기
	list_insert_ordered (&thread_current()->locks, &(lock->elem), lock_priority_desc, NULL);
	lock->holder = thread_current ();
	// 기부 후 wanted_lock 초기화
	thread_current()->wanted_lock = NULL;
}


void 
donate_priority(struct lock *lock) {
	if (lock != NULL && lock->holder != NULL) {
		struct thread *lock_holder = lock->holder;

		// 현재 스레드의 우선순위가 lock의 max_priority보다 크면 업데이트
		if (lock->max_priority < thread_get_priority()) {
			lock->max_priority = thread_get_priority();

			// lock을 보유한 스레드가 존재하고, 그 스레드의 우선순위가 현재 스레드보다 낮으면
			if (thread_any_priority(lock_holder) < thread_get_priority()) {
				// 우선순위 기부
				lock_holder->donated_priority = thread_get_priority();

				// 페이지 폴트 수정
				if (!list_empty(&lock_holder->locks)) {
					// lock_holder가 보유한 가장 높은 우선순위의 락이 현재 스레드의 우선순위보다 낮으면
					if (list_entry(list_front(&lock_holder->locks), struct lock, elem)->max_priority < thread_get_priority()) {
						// 락 리스트에서 현재 락을 제거하고 다시 정렬된 위치에 삽입
						list_remove(&lock->elem);
						list_insert_ordered(&lock_holder->locks, &lock->elem, lock_priority_desc, NULL);
					}
				}
			}
		}
		// 디버깅을 잘하고 조건을 잘보자
		if(lock->holder->wanted_lock != NULL && lock->holder->wanted_lock->max_priority < thread_get_priority()) {
			donate_priority(lock_holder->wanted_lock);
		}
	}
}

/* Tries to acquires LOCK and returns true if successful or false
   on failure.  The lock must not already be held by the current
   thread.

   This function will not sleep, so it may be called within an
   interrupt handler. */
bool
lock_try_acquire (struct lock *lock) {
	bool success;

	ASSERT (lock != NULL);
	ASSERT (!lock_held_by_current_thread (lock));

	success = sema_try_down (&lock->semaphore);
	if (success)
		lock->holder = thread_current ();
	return success;
}

/* Releases LOCK, which must be owned by the current thread.
   This is lock_release function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to release a lock within an interrupt
   handler. */
void
lock_release (struct lock *lock) {
	ASSERT (lock != NULL);
	ASSERT (lock_held_by_current_thread (lock));

	list_remove(&lock->elem);
	struct thread *lock_holder = lock->holder;

	if(lock_holder != NULL) {
		if (list_empty(&lock_holder->locks)) {
			lock_holder->donated_priority = PRI_DNTD_INIT;
		} else {
			lock_holder -> donated_priority = list_entry(list_front(&lock_holder->locks), struct lock, elem)->max_priority;
		}
	}

	lock->max_priority = PRI_DNTD_INIT;
	lock->holder = NULL;
	sema_up (&lock->semaphore);
}

/* Returns true if the current thread holds LOCK, false
   otherwise.  (Note that testing whether some other thread holds
   a lock would be racy.) */
bool
lock_held_by_current_thread (const struct lock *lock) {
	ASSERT (lock != NULL);

	return lock->holder == thread_current ();
}

/* One semaphore in a list. */
struct semaphore_elem {
	struct list_elem elem;              /* List element. */
	struct semaphore semaphore;         /* This semaphore. */
};

/* Initializes condition variable COND.  A condition variable
   allows one piece of code to signal a condition and cooperating
   code to receive the signal and act upon it. */
void
cond_init (struct condition *cond) {
	ASSERT (cond != NULL);

	list_init (&cond->waiters);
}

/* Atomically releases LOCK and waits for COND to be signaled by
   some other piece of code.  After COND is signaled, LOCK is
   reacquired before returning.  LOCK must be held before calling
   this function.

   The monitor implemented by this function is "Mesa" style, not
   "Hoare" style, that is, sending and receiving a signal are not
   an atomic operation.  Thus, typically the caller must recheck
   the condition after the wait completes and, if necessary, wait
   again.

   A given condition variable is associated with only a single
   lock, but one lock may be associated with any number of
   condition variables.  That is, there is a one-to-many mapping
   from locks to condition variables.

   This function may sleep, so it must not be called within an
   interrupt handler.  This function may be called with
   interrupts disabled, but interrupts will be turned back on if
   we need to sleep. */
void
cond_wait (struct condition *cond, struct lock *lock) {
	struct semaphore_elem waiter;

	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	sema_init (&waiter.semaphore, 0);
	list_push_back (&cond->waiters, &waiter.elem);
	lock_release (lock);
	sema_down (&waiter.semaphore);
	lock_acquire (lock);
}

/* If any threads are waiting on COND (protected by LOCK), then
   this function signals one of them to wake up from its wait.
   LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_signal (struct condition *cond, struct lock *lock UNUSED) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);
	ASSERT (!intr_context ());
	ASSERT (lock_held_by_current_thread (lock));

	if (!list_empty (&cond->waiters))
		sema_up (&list_entry (list_pop_front (&cond->waiters),
					struct semaphore_elem, elem)->semaphore);
}

/* Wakes up all threads, if any, waiting on COND (protected by
   LOCK).  LOCK must be held before calling this function.

   An interrupt handler cannot acquire a lock, so it does not
   make sense to try to signal a condition variable within an
   interrupt handler. */
void
cond_broadcast (struct condition *cond, struct lock *lock) {
	ASSERT (cond != NULL);
	ASSERT (lock != NULL);

	while (!list_empty (&cond->waiters))
		cond_signal (cond, lock);
}
