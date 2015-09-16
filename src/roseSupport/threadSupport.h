/* Thread support for ROSE */
#ifndef ROSE_threadSupport_H
#define ROSE_threadSupport_H

/* Design rules:
 *   1. All public symbols in this file should use the "RTS_" prefix (ROSE Thread Support).
 *
 *   2. All constructs that have a user-supplied compound statement as a "body", have a matching "_END" macro.  For instance,
 *      RTS_MUTEX starts a mutual exclusion for a critical section, which ends with an RTS_MUTEX_END macro.  The END macros
 *      generally take no arguments.
 *
 *   3. Locally scoped symbols defined by the macros have names beginning with "RTS_". This is generally followed by the first
 *      letter of the rest of the macro name, a letter "s" or "p" for "shared" or "private", and an underscore.  For example,
 *      within the RTS_MUTEX macros, a private (non-shared) variable might be named "RTS_Mp_mutex".
 *
 *   4. Constructs that allow a user-supplied compound statement as a "body" should allow the body to "break" or "throw". Both
 *      forms of premature exit should behave as if the body executed to completion (except throw will throw the exception
 *      again automatically).
 *
 *   5. New functionality shall have types, constants, and functions reminiscent of the POSIX threads interface, but whose
 *      names begin with "RTS_" rather than "pthread_".
 */

/* Needed for ROSE_HAVE_PTHREAD_H definition */
#include "rosePublicConfig.h"

#include <assert.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#include <string>

/* Figure out whether ROSE can support multi-threading and what kind of support library is available. */
#ifdef _REENTRANT                                       // Does user want multi-thread support? (e.g., g++ -pthread)
# ifdef ROSE_HAVE_PTHREAD_H                             // Do we have POSIX threads? Consider using Boost Threads instead.
#  define ROSE_THREADS_ENABLED
#  define ROSE_THREADS_POSIX
#  include <pthread.h>
# else
#  undef  ROSE_THREADS_ENABLED
# endif
#else
# undef ROSE_THREADS_ENABLED
#endif

/* The __attribute__ mechanism is only supported by GNU compilers */
#ifndef __GNUC__
#define  __attribute__(x)  /*NOTHING*/
#define  __attribute(x)    /*NOTHING*/
#endif

/******************************************************************************************************************************
 *                                      Layered Synchronization Primitives
 ******************************************************************************************************************************/

/** Layers where syncrhonization primitives are defined.
 *
 *  When a thread intends to acquire multiple locks at a time, it must acquire those locks in a particular order to prevent
 *  deadlock.  Deadlock can occur when thread 1 attempts to acquire lock A and then B, while thread 2 attempts to acquire lock
 *  B and then A.  By defining every lock to belong to a particular software layer, we can impose a partial ordering on the
 *  locks and enforce the requirement that a thread obtain locks in that order.  To use the previous example, if lock A belongs
 *  to layer X and lock B to layer Y, then a rule that says "locks of layer X must be acquired before locks of layer Y when
 *  attempting to acquire both at once" would be sufficient to prevent deadlock.  This mechanism makes no attempt to define an
 *  acquisition order for locks of the same layer (at least not at this time).
 *
 *  When a thread acquires locks from more than one layer at a time, they must be acquired in descending order by layer (they
 *  can be released in any order).  If a thread attempts to aquire a lock whose layer is greater than the minimum layer for
 *  which it already holds a lock, then an error message is emitted and the process aborts.
 *
 *  New layers can be added to this enum and the RTS_LAYER_NLAYERS constant can be increased if necessary.  When a layer's
 *  number is changed, all of ROSE must be recompiled.  The constant name is used in error messages. Names ending with "_CLASS"
 *  refer to synchronization primities that are class data members (or global), while those ending with "_OBJ" belong to a
 *  particular object.
 *
 *  Layer zero is special and is the default layer for all syncronization primitives not explicitly associated with any layer.
 *  Locks in layer zero can be acquired in any order without generating an error message (so silent deadlock is a distinct
 *  possibility). */
enum RTS_Layer {
    RTS_LAYER_DONTCARE = 0,

    /* ROSE library layers, 100-199 */
    RTS_LAYER_ROSE_CALLBACKS_LIST_OBJ   = 100,          /**< ROSE_Callbacks::List class */
    RTS_LAYER_DISASSEMBLER_CLASS        = 110,          /**< Disassembler class */
    RTS_LAYER_ROSE_SMT_SOLVERS          = 115,          /**< SMTSolver class */

    /* Simulator layers (see projects/simulator), 200-220
     *
     * Constraints:
     *     RSIM_PROCESS_OBJ        < RSIM_PROCESS_CLONE_OBJ
     *     RSIM_SIGNALHANDLING_OBJ < RSIM_PROCESS_OBJ
     */

    /* User layers.  These are for people that might want to use the ROSE Thread Support outside ROSE, such as in ROSE
     * projects.   We leave it up to them to organize how they'll use the available layers. */
    RTS_LAYER_USER_MIN                  = 250,          /**< Minimum layer for end-user usage. */
    RTS_LAYER_USER_MAX                  = 299,          /**< Maximum layer for end-user usage. */

    /* Max number of layers (i.e., 0 through N-1) */
    RTS_LAYER_NLAYERS                   = 300
};

/** Check for layering violations.  This should be called just before any attempt to acquire a lock.  The specified layer
 *  should be the layer of the lock being acquired.  Returns true if it is OK to acquire the lock, false if doing so could
 *  result in deadlock.  Before returning false, an error message is printed to stderr.
 *
 *  Note that this function is a no-op when the compiler does not support the "__thread" type qualifier, nor any other
 *  qualifier as detected by the ROSE configure script.  Currently, this is a no-op on Mac OS X. [RPM 2011-05-04] */
bool RTS_acquiring(RTS_Layer);

/** Notes the release of a lock.  This function should be called before or after each release of a lock.  The layer number is
 *  that of the lock which is release. */
void RTS_releasing(RTS_Layer);

/******************************************************************************************************************************
 *                                      Paired macros for using mutual exclusion locks
 ******************************************************************************************************************************/

/** Protect a critical section with a mutual exclusion lock.
 *
 *  This macro should be used within ROSE whenever we need to obtain a lock for a critical section. The critical section should
 *  end with a matching RTS_MUTEX_END macro.  The suggested code style is to use curly braces and indentation to help visually
 *  line up the RTS_MUTEX with the RTS_MUTEX_END, such as:
 *
 *  @code
 *  RTS_MUTEX(class_mutex) {
 *      critical_section_goes_here;
 *  } RTS_MUTEX_END;
 *  @endcode
 *
 *  The critical section should not exit the construct except through the RTS_MUTEX_END macro.  In other words, the critical
 *  section should not have "return" statements, longjmps, or any "goto" that branches outside the critical section.  However,
 *  "break" statements and exceptions are supported.
 *
 *  If the mutex is an error checking mutex then ROSE will assert that the lock is not already held by this thread. If the
 *  mutex is recursive then the lock will be obtained recursively if necessary.
 */
#ifdef ROSE_THREADS_ENABLED
#  define RTS_MUTEX(MUTEX)                                                                                                     \
    do {        /* standard CPP macro protection */                                                                            \
        RTS_mutex_t *RTS_Mp_mutex = &(MUTEX); /* saved for when we need to unlock it */                                        \
        int RTS_Mp_err = RTS_mutex_lock(RTS_Mp_mutex);                                                                         \
        assert(0==RTS_Mp_err);                                                                                                 \
        do {    /* so we can catch "break" statements */                                                                       \
            try {

/** End an RTS_MUTEX construct. */
#  define RTS_MUTEX_END                                                                                                        \
            } catch (...) {                                                                                                    \
                RTS_Mp_err = RTS_mutex_unlock(RTS_Mp_mutex);                                                                   \
                assert(0==RTS_Mp_err);                                                                                         \
                throw;                                                                                                         \
            }                                                                                                                  \
        } while (0);                                                                                                           \
        RTS_Mp_err = RTS_mutex_unlock(RTS_Mp_mutex);                                                                           \
        assert(0==RTS_Mp_err);                                                                                                 \
    } while (0)
#else
#  define RTS_MUTEX(MUTEX)                                                                                                     \
    do {
#  define RTS_MUTEX_END                                                                                                        \
    } while (0)
#endif

/******************************************************************************************************************************
 *                                      Types and functions for mutexes
 ******************************************************************************************************************************/

#define RTS_MUTEX_MAGIC 0x1a95a713

#ifdef ROSE_THREADS_ENABLED

/** Mutual exclusion lock.
 *
 *  This struct is intended to provide a portable implementation of mutual exclusion locks (mutexes).  It should be used in a
 *  manner similar to POSIX Threads' pthread_mutex_t. */
struct RTS_mutex_t {
    unsigned magic;
    RTS_Layer layer;
    pthread_mutex_t mutex;
};

#define RTS_MUTEX_INITIALIZER(LAYER) { RTS_MUTEX_MAGIC, (LAYER), PTHREAD_MUTEX_INITIALIZER }

/** Initialize a mutual exclusion lock. */
int RTS_mutex_init(RTS_mutex_t*, RTS_Layer, pthread_mutexattr_t*);

#else

struct RTS_mutex_t {
    int dummy;
};

#define RTS_MUTEX_INITIALIZER(LAYER) { 0 }

int RTS_mutex_init(RTS_mutex_t*, RTS_Layer, void*);

#endif

/** Obtain an exclusive lock.  Behavior is similar to pthread_mutex_lock().  Returns zero on success, errno on failure. */
int RTS_mutex_lock(RTS_mutex_t*);

/** Release an exclusive lock.  Behavior is similar to pthread_mutex_unlock(). Returns zero on success, errno on failure. */
int RTS_mutex_unlock(RTS_mutex_t*);




/*******************************************************************************************************************************
 *                                      Paired macros for using RTS_rwlock_t
 *******************************************************************************************************************************/

/** Protect a critical section with a read-write lock.
 *
 *  These macros should be used within ROSE whenever we need to protect a critical section among two kinds of access: reading
 *  and writing.
 *
 *  This construct allows at most one thread to hold a write lock, or multiple threads to hold read locks.  Write locks are
 *  granted only when no other thread holds a read or write lock, and a request for a write lock blocks (becomes pending) if
 *  the lock cannot be granted.  Read locks are granted only when no write lock is already granted to another thread, and no
 *  write lock is pending.
 *
 *  Like POSIX read-write locks, RTS_rwlock_t allows a single thread to obtain multiple read locks recursively. Unlike POSIX
 *  read-write locks, RTS_rwlock_t also allows the following:
 *
 *  <ul>
 *    <li>A lock (read or write) is granted to a thread which already holds a write lock.  The POSIX implementation deadlocks
 *        in this situation.  This feature is useful in ROSE when a read-write lock guards access to data members of an object,
 *        and the object methods can be invoked recursively.</li>
 *    <li>The RTS_rwlock_unlock() function releases locks in the reverse order they were granted and should only be called by
 *        the thread to which the lock was granted.</li>
 *  </ul>
 *
 *  In particular, this implementation does not allow a thread which holds only read locks to be granted a write lock (i.e., no
 *  lock upgrading). Like POSIX read-write locks, this situation will lead to deadlock.
 *
 *  The RTS_READ macro should be paired with an RTS_READ_END macro; the RTS_WRITE macro should be paired with an RTS_WRITE_END
 *  macro.  The RTS_RWLOCK macro is a generalization of RTS_READ and RTS_WRITE where its second argument is either the word
 *  "rdlock" or "wrlock", respectively. It should be paired with an RTS_RWLOCK_END macro.
 *
 *  The critical section may exit only via "break" statement, throwing an exception, or falling through the end.  Exceptions
 *  thrown by the critical section will release the lock before rethrowing the exception.
 *
 *  A simple example demonstrating how locks can be obtained recursively.  Any number of threads can be operating on a single,
 *  common object concurrently and each of the four defined operations remains atomic.
 *
 *  @code
 *  class Stack {
 *  public:
 *      Stack() {
 *          RTS_rwlock_init(&rwlock, NULL);
 *      }
 *  
 *      MD5sum sum() const {
 *          MD5sum retval;
 *          RTS_READ(rwlock) {
 *              for (size_t i=0; i<stack.size(); i++)
 *                  retval.composite(stack[i]);
 *          } RTS_READ_END;
 *          return retval;
 *      }
 *
 *      void push(const std::string &s) {
 *          RTS_WRITE(rwlock) {
 *              stack.push_back(s);
 *          } RTS_WRITE_END;
 *      }
 *  
 *      // swap top and third from top; toss second from top
 *      // throw exception when stack becomes too small
 *      void adjust() {
 *          RTS_WRITE(rwlock) {
 *              std::string s1 = pop(); // may throw
 *              (void) pop();           // may throw
 *              std::string s2 = pop(); // may throw
 *              push(s1);
 *              push(s2);
 *          } RTS_WRITE_END;
 *      }
 *
 *      // adjust() until sum specified termination condition
 *      // throw exception when stack becomes too small
 *      void adjust_until(const MD5sum &term) {
 *          RTS_WRITE(rwlock) {
 *              while (sum()!=term)    // recursive read lock
 *                  adjust();          // recursive write lock; may throw
 *          } RTS_WRITE_END;
 *      }
 *  
 *  private:
 *      std::vector<std::string> stack;
 *      mutable RTS_rwlock_t rwlock;   // mutable so sum() can be const as user would expect
 *  };
 *  @endcode
 */
#ifdef ROSE_THREADS_ENABLED
#  define RTS_RWLOCK(RWLOCK, HOW)                                                                                              \
    do {        /* standard CPP macro protection */                                                                            \
        RTS_rwlock_t *RTS_Wp_rwlock = &(RWLOCK); /* saved for when we need to unlock it */                                     \
        int RTS_Wp_err = RTS_rwlock_##HOW(RTS_Wp_rwlock);                                                                      \
        assert(0==RTS_Wp_err);                                                                                                 \
        do {    /* so we can catch "break" statements */                                                                       \
            try {
                
#  define RTS_RWLOCK_END                                                                                                       \
            } catch (...) {                                                                                                    \
                RTS_Wp_err = RTS_rwlock_unlock(RTS_Wp_rwlock);                                                                 \
                assert(0==RTS_Wp_err);                                                                                         \
                throw;                                                                                                         \
            }                                                                                                                  \
        } while (0);                                                                                                           \
        RTS_Wp_err = RTS_rwlock_unlock(RTS_Wp_rwlock);                                                                         \
        assert(0==RTS_Wp_err);                                                                                                 \
    } while (0)

#else
#  define RTS_RWLOCK(RWLOCK, HOW)                                                                                              \
    do {
#  define RTS_RWLOCK_END                                                                                                       \
    } while (0)
#endif

#define RTS_READ(RWLOCK)        RTS_RWLOCK(RWLOCK, rdlock)                      /**< See RTS_RWLOCK. */
#define RTS_READ_END            RTS_RWLOCK_END                                  /**< See RTS_RWLOCK. */
#define RTS_WRITE(RWLOCK)       RTS_RWLOCK(RWLOCK, wrlock)                      /**< See RTS_RWLOCK. */
#define RTS_WRITE_END           RTS_RWLOCK_END                                  /**< See RTS_RWLOCK. */

/*******************************************************************************************************************************
 *                                      Types and functions for RTS_rwlock_t
 *
 * Programmers should generally use the RTS_READ and RTS_WRITE macros in their source code. The symbols defined here are
 * similar to pthread_rwlock* symbols and are mostly to support RTS_READ and RTS_WRITE macros (like how the RTS_mutex*
 * symbols support the RTS_MUTEX macro).
 *******************************************************************************************************************************/

/** A read-write lock for ROSE Thread Support.  As with POSIX Thread types, this type should be treated as opaque and
 *  initialized with RTS_RWLOCK_INITIALIZER or RTS_rwlock_init().  It is used as the argument for RTS_READ and RTS_WRITE
 *  macros. */
#ifdef ROSE_THREADS_ENABLED
struct RTS_rwlock_t {
    unsigned magic;                             /* always RTS_RWLOCK_MAGIC after initialization */
    RTS_Layer layer;                            /* software layer to which this lock belongs, or zero */
    pthread_rwlock_t rwlock;                    /* the main read-write lock */
    RTS_mutex_t mutex;                          /* mutex to protect the following data members */
    size_t nlocks;                              /* number of write locks held */
    pthread_t owner;                            /* thread that currently holds the write lock */
};
#else
struct RTS_rwlock_t {
    int dummy;
};
#endif

#define RTS_RWLOCK_MAGIC 0x20e7f3f4

/** Static initializer for an RTS_rwlock_t instance, similar in nature to PTHREAD_RWLOCK_INITIALIZER. */
#ifdef ROSE_THREADS_ENABLED
#  define RTS_RWLOCK_INITIALIZER(LAYER) { RTS_RWLOCK_MAGIC,                                                                    \
                                          (LAYER),                                                                             \
                                          PTHREAD_RWLOCK_INITIALIZER,                                                          \
                                          RTS_MUTEX_INITIALIZER(RTS_LAYER_DONTCARE),                                           \
                                          0/*...*/                                                                             \
                                         }
#else
#  define RTS_RWLOCK_INITIALIZER(LAYER) { 0 }
#endif

#ifdef ROSE_THREADS_ENABLED
/** Intializes an RTS_rwlock_t in a manner similar to pthread_rwlock_init(). */
int RTS_rwlock_init(RTS_rwlock_t *rwlock, RTS_Layer, pthread_rwlockattr_t *wrlock_attrs);
#else
int RTS_rwlock_init(RTS_rwlock_t *rwlock, RTS_Layer, void                 *wrlock_attrs);
#endif
              
/** Obtain a read lock.
 *
 *  The semantics are identical to pthread_rwlock_rdlock() documented in "The Open Group Base Specifications Issue 6: IEEE Std
 *  1003.1, 2004 Edition" [1] with the following changes:
 *
 *  <ul>
 *    <li>If the calling thread holds a write lock, then the read lock is automatically granted.  A single process can be both
 *        reading and writing, and may hold multiple kinds locks.</li>
 *    <li>Calls to RTS_rwlock_unlock() release the recursive locks in the opposite order they were obtained.</li>
 *  </ul>
 *
 *  [1] http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_rwlock_rdlock.html
 */
int RTS_rwlock_rdlock(RTS_rwlock_t *rwlock);

/** Obtain a write lock.
 * 
 *  The semantics are identical to pthread_rwlock_wrlock() documented in "The Open Group Base Specifications Issue 6: IEEE Std
 *  1003.1, 2004 Edition" [1] with the following changes:
 *
 *  <ul>
 *    <li>If the calling thread already holds a write lock, then a recursive write lock is automatically granted.  A single
 *        process can hold multiple write locks.</li>
 *    <li>Calls to RTS_rwlock_unlock() release the recursive locks in the opposite order they were obtained.</li>
 *  </ul>
 *
 *  Note that a write lock will not be granted if a thread already holds only read locks.  Attempting to obtain a write lock in
 *  this situation will result in deadlock.
 *
 *  [1] http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_rwlock_wrlock.html
 */
int RTS_rwlock_wrlock(RTS_rwlock_t *rwlock);

/** Release a read or write lock.
 *
 *  The semantics are identical to pthread_rwlock_unlock() documented in "The Open Group Base Specification Issue 6: IEEE Std
 *  1003.1, 2004 Edition" [1] with the following additions:
 *
 *  <ul>
 *    <li>Recursive locks are released in the oposite order they were acquired.</li>
 *  </ul>
 *
 *  [1] http://pubs.opengroup.org/onlinepubs/009695399/functions/pthread_rwlock_unlock.html
 */
int RTS_rwlock_unlock(RTS_rwlock_t *rwlock);



/*******************************************************************************************************************************
 *                                      Paired macros for initialization functions
 *******************************************************************************************************************************/


/** Initializer synchronization.
 *
 *  Sometimes we want a critical section to be executed only by the first thread to call the function and all other threads
 *  that might call the same function should block until the first caller completes.  These macros can be used for that
 *  purpose.
 *
 *  The MUTEX is briefly locked to inspect the state of initilization, and then unlocked before the user-supplied body is
 *  executed.  The user is permitted to obtain the mutex lock again in the body if desired, although this is only necessary if
 *  other code paths (outside the RTS_INIT construct) might interfere with the body.
 *
 *  If ALLOW_RECURSION is true, then a recursive call from the body will jump over the RTS_INIT construct without doing
 *  anything (other than briefly obtaining the mutex lock to inspect the state of initialization). Otherwise a recursive call
 *  from the body is considered a logic error and the process will be aborted. For convenience, we define two additional
 *  macros, RTS_INIT_RECURSIVE and RTS_INIT_NONRECURSIVE, which may be used in place of the two-argument RTS_INIT macro.
 *
 *  The user-supplied body may exit prematurely either by a "break" statement or by throwing an exception.  In either case, the
 *  initialization is assumed to have completed and the body will not be executed by any other future call.  Any other kind of
 *  premature exit from the body (return, goto, longjmp, etc) results in undefined behavior.
 *
 *  Example code. Consider a class method which is responsible for one-time initialization of certain class data
 *  structures. This initialization function is called by nearly every other method in the class, and therefore anything that
 *  the initialization function does might result in a recursive call to the initializer.
 *
 *  @code
 *  static void
 *  SomeClass::initclass()
 *  {
 *      RTS_INIT_RECURSIVE(class_mutex) {
 *          register_subclass(new Subclass1);
 *          register_subclass(new Subclass2);
 *          register_subclass(new Subclass3);
 *      } RTS_INIT_END;
 *  }
 *  @endcode
 */
#ifdef ROSE_THREADS_ENABLED
#  define RTS_INIT(MUTEX, ALLOW_RECURSION)                                                                                     \
    do {                                                                                                                       \
        static bool RTS_Is_initialized=false, RTS_Is_initializing=false;        /* "s"==shared; "p"=private */                 \
        static pthread_t RTS_Is_initializer;                                                                                   \
        static pthread_cond_t RTS_Is_condition=PTHREAD_COND_INITIALIZER;                                                       \
        RTS_mutex_t *RTS_Ip_mutex = &(MUTEX);                                                                                  \
        bool RTS_Ip_initialized, RTS_Ip_initializing;                                                                          \
        bool RTS_Ip_allow_recursion = (ALLOW_RECURSION);                                                                       \
                                                                                                                               \
        /* First critical section is only to obtain the initialization status and update it to "initializing" if necessary. We \
         * must release the lock before the RTS_I_LOCK body is executed in case we need to handle recursive calls to the       \
         * RTS_I_LOCK construct. */                                                                                            \
        RTS_MUTEX(MUTEX) {                                                                                                     \
            if (!(RTS_Ip_initialized=RTS_Is_initialized) && !(RTS_Ip_initializing=RTS_Is_initializing)) {                      \
                RTS_Is_initializing = true; /* but leave private copy false so we can detect changed state */                  \
                RTS_Is_initializer = pthread_self();                                                                           \
            }                                                                                                                  \
        } RTS_MUTEX_END;                                                                                                       \
                                                                                                                               \
        if (!RTS_Ip_initialized) {                                                                                             \
            if (!RTS_Ip_initializing) {                                                                                        \
                do { /* so we catch "break" statements in user-supplied code. */                                               \
                    try {


/** End an RTS_INIT construct. */
#  define RTS_INIT_END                                                                                                         \
                    } catch (...) {                                                                                            \
                        /* If the user supplied body throws an exception, consider the body to have completed. */              \
                        RTS_MUTEX(*RTS_Ip_mutex) {                                                                             \
                            RTS_Is_initialized = true;                                                                         \
                            RTS_Is_initializing = false;                                                                       \
                            pthread_cond_broadcast(&RTS_Is_condition);                                                         \
                        } RTS_MUTEX_END;                                                                                       \
                        throw;                                                                                                 \
                    }                                                                                                          \
                } while (0);                                                                                                   \
                RTS_MUTEX(*RTS_Ip_mutex) {                                                                                     \
                    RTS_Is_initialized = true;                                                                                 \
                    RTS_Is_initializing = false;                                                                               \
                    pthread_cond_broadcast(&RTS_Is_condition);                                                                 \
                } RTS_MUTEX_END;                                                                                               \
            } else if (pthread_equal(pthread_self(), RTS_Is_initializer)) {                                                    \
                /* This was a recursive call resulting from the user-supplied RTS_I body and we must allow it to continue      \
                 * unimpeded to prevent deadlock. We don't need to be holding the MUTEX lock to access RTS_Is_initializer      \
                 * because it's initialized only the first time through the critical section above--and the only way to reach  \
                 * this point is by this or some other thread having already initialized RTS_Is_initializer. */                \
                if (!RTS_Ip_allow_recursion) {                                                                                 \
                    fprintf(stderr, "RTS_I_LOCK body made a recursive call. Aborting...\n");                                   \
                    abort();                                                                                                   \
                }                                                                                                              \
            } else {                                                                                                           \
                /* This is some other thread which is not in charge of initializing the class, bug which arrived to the        \
                 * RTS_I_LOCK before the first thread completed the initialization. */                                         \
                RTS_MUTEX(*RTS_Ip_mutex) {                                                                                     \
                    while (!RTS_Is_initialized)                                                                                \
                        pthread_cond_wait(&RTS_Is_condition, &(RTS_Ip_mutex->mutex));                                          \
                } RTS_MUTEX_END;                                                                                               \
            }                                                                                                                  \
        }                                                                                                                      \
    } while (0)
#else
#  define RTS_INIT(MUTEX, ALLOW_RECURSION)                                                                                     \
    do {                                                                                                                       \
        static bool RTS_Is_initialized=false;                                                                                  \
        if (!RTS_Is_initialized) {                                                                                             \
            RTS_Is_initialized = true;                                                                                         \
            do {
#  define RTS_INIT_END                                                                                                         \
            } while (0);                                                                                                       \
        }                                                                                                                      \
    } while (0)
#endif

#define RTS_INIT_RECURSIVE(MUTEX)       RTS_INIT(MUTEX, true)                   /**< See RTS_INIT */
#define RTS_INIT_NONRECURSIVE(MUTEX)    RTS_INIT(MUTEX, false)                  /**< See RTS_INIT */

#endif /* !ROSE_threadSupport_H !*/
