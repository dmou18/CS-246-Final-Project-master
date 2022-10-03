#include <types.h>
#include <lib.h>
#include <synchprobs.h>
#include <synch.h>
#include <opt-A1.h>

/* 
 * This simple default synchronization mechanism allows only vehicle at a time
 * into the intersection.   The intersectionSem is used as a a lock.
 * We use a semaphore rather than a lock so that this code will work even
 * before locks are implemented.
 */

/* 
 * Replace this default synchronization mechanism with your own (better) mechanism
 * needed for your solution.   Your mechanism may use any of the available synchronzation
 * primitives, e.g., semaphores, locks, condition variables.   You are also free to 
 * declare other global variables if your solution requires them.
 */

/*
 * replace this with declarations of any synchronization and other variables you need here
 */

struct cv *Ncv;
struct cv *Ecv;
struct cv *Scv;
struct cv *Wcv;
struct lock *directlock;
volatile bool Blocked;
//volatile bool notFirst;
volatile int Car;
volatile int Nwait;
volatile int Ewait;
volatile int Swait;
volatile int Wwait;

/* 
 * The simulation driver will call this function once before starting
 * the simulation
 *
 * You can use it to initialize synchronization and other variables.
 * 
 */
void
intersection_sync_init(void)
{
 
  Ncv = cv_create("Ncv");
  Ecv = cv_create("Ecv");
  Scv = cv_create("Scv");
  Wcv = cv_create("Wcv");
  directlock = lock_create("directlock");
  Blocked = false;
 // notFirst = false;
  Car = 0;
  Nwait = 0;
  Ewait = 0;
  Swait = 0;
  Wwait = 0;

  if (Ncv == NULL) {
    panic("could not create Ncv");
  }
  if (Scv == NULL) {
    panic("could not create Scv");
  }
  if (Wcv == NULL) {
    panic("could not create Wcv");
  }
  if (Ecv == NULL) {
    panic("could not create Ecv");
  }
  if (directlock == NULL) {
    panic("could not create directlock");
  }
  return;
}

/* 
 * The simulation driver will call this function once after
 * the simulation has finished
 *
 * You can use it to clean up any synchronization and other variables.
 *
 */
void
intersection_sync_cleanup(void)
{
  /* replace this default implementation with your own implementation */
/*  KASSERT(Nsem != NULL);
  KASSERT(Ssem != NULL);
  KASSERT(Wsem != NULL);
  KASSERT(Esem != NULL);
  KASSERT(directlock != NULL);
*/

  lock_destroy(directlock);
  cv_destroy(Ncv);
  cv_destroy(Scv);
  cv_destroy(Wcv);
  cv_destroy(Ecv);

}


/*
 * The simulation driver will call this function each time a vehicle
 * tries to enter the intersection, before it enters.
 * This function should cause the calling simulation thread 
 * to block until it is OK for the vehicle to enter the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle is arriving
 *    * destination: the Direction in which the vehicle is trying to go
 *
 * return value: none
 */

void
intersection_before_entry(Direction origin, Direction destination) 
{ 
  
/*  KASSERT(Nsem != NULL);
  KASSERT(Ssem != NULL);
  KASSERT(Wsem != NULL);
  KASSERT(Esem != NULL);
  KASSERT(directlock != NULL);
*/

  lock_acquire(directlock);
  (void) destination;

  if (Blocked == false) {
    Blocked = true;
    Car++;
  } 
  else 
  {
    switch (origin) 
    {
        case north:
            Nwait++;
            cv_wait(Ncv, directlock);
            if (Blocked == false) {
                Blocked = true;
            }
            Car++;
            Nwait--;
            break;

        case east:
            Ewait++;
            cv_wait(Ecv, directlock);
            if (Blocked == false) {
                Blocked = true;
            }
            Car++;
            Ewait--;
            break;
        
        case south:
            Swait++;
            cv_wait(Scv, directlock);
            if (Blocked == false) {
                Blocked = true;
            }
            Car++;
            Swait--;
            break;
        
        case west:
            Wwait++;
            cv_wait(Wcv, directlock);
            if (Blocked == false) {
                Blocked = true;
            }
            Car++;
            Wwait--;
            break;
    }
  }
  lock_release(directlock);
}


/*
 * The simulation driver will call this function each time a vehicle
 * leaves the intersection.
 *
 * parameters:
 *    * origin: the Direction from which the vehicle arrived
 *    * destination: the Direction in which the vehicle is going
 *
 * return value: none
 */

void
intersection_after_exit(Direction origin, Direction destination) 
{
  lock_acquire(directlock);
  (void) destination;
  Car--;
/*  if (Car != 0) {
    switch (origin) 
    {
        case north:
            cv_broadcast(Ncv, directlock);
            break;

        case east:
            cv_broadcast(Ecv, directlock);
            break;

        case south:
            cv_broadcast(Scv, directlock);
            break;

        case west:
            cv_broadcast(Wcv, directlock);
            break;
    }
  }
  */
  if (Car == 0) {
    Blocked = false;
    switch (origin) 
    {
        case north:
            if (Ewait != 0) {
                Blocked = true;
                cv_broadcast(Ecv, directlock);
            } 
            else if (Swait != 0) {
                Blocked = true;
                cv_broadcast(Scv, directlock);
            } 
            else if (Wwait != 0) {
                Blocked = true;
                cv_broadcast(Wcv, directlock);
            }
            else if (Nwait != 0) {
                Blocked = true;
                cv_broadcast(Ncv, directlock);
            }
            break;

        case east:
            if (Swait != 0) {
                Blocked = true;
                cv_broadcast(Scv, directlock);
            } 
            else if (Wwait != 0) {
                Blocked = true;
                cv_broadcast(Wcv, directlock);
            } 
            else if (Nwait != 0) {
                Blocked = true;
                cv_broadcast(Ncv, directlock);
            }
            else if (Ewait != 0) {
                Blocked = true;
                cv_broadcast(Ecv, directlock);
            }
            break;

        case south:
            if (Wwait != 0) {
                Blocked = true;
                cv_broadcast(Wcv, directlock);
            } 
            else if (Nwait != 0) {
                Blocked = true;
                cv_broadcast(Ncv, directlock);
            } 
            else if (Ewait != 0) {
                Blocked = true;
                cv_broadcast(Ecv, directlock);
            }
            else if (Swait != 0) {
                Blocked = true;
                cv_broadcast(Scv, directlock);
            }
            break;

        case west:
            if (Nwait != 0) {
                Blocked = true;
                cv_broadcast(Ncv, directlock);
            } 
            else if (Ewait != 0) {
                Blocked = true;
                cv_broadcast(Ecv, directlock);
            } 
            else if (Swait != 0) {
                Blocked = true;
                cv_broadcast(Scv, directlock);
            }
            else if (Wwait != 0) {
                Blocked = true;
                cv_broadcast(Wcv, directlock);
            }
            break;
    }
  }
  lock_release(directlock); 
}
