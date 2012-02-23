// exception.cc
//      Entry point into the Nachos kernel from user programs.
//      There are two kinds of things that can cause control to
//      transfer back to here from user code:
//
//      syscall -- The user code explicitly requests to call a procedure
//      in the Nachos kernel.  Right now, the only function we support is
//      "Halt".
//
//      exceptions -- The user code does something that the CPU can't handle.
//      For instance, accessing memory that doesn't exist, arithmetic errors,
//      etc.
//
//      Interrupts (which can also cause control to transfer from user
//      code into the Nachos kernel) are handled elsewhere.
//
// For now, this only handles the Halt() system call.
// Everything else core dumps.
//
// Copyright (c) 1992-1993 The Regents of the University of California.
// All rights reserved.  See copyright.h for copyright notice and limitation
// of liability and disclaimer of warranty provisions.

#include "copyright.h"
#include "system.h"
#include "syscall.h"
#include "synchconsole.h"

//----------------------------------------------------------------------
// UpdatePC : Increments the Program Counter register in order to resume
// the user program immediately after the "syscall" instruction.
//----------------------------------------------------------------------
static void
UpdatePC ()
{
    int pc = machine->ReadRegister (PCReg);
    machine->WriteRegister (PrevPCReg, pc);
    pc = machine->ReadRegister (NextPCReg);
    machine->WriteRegister (PCReg, pc);
    pc += 4;
    machine->WriteRegister (NextPCReg, pc);
}


//----------------------------------------------------------------------
// ExceptionHandler
//      Entry point into the Nachos kernel.  Called when a user program
//      is executing, and either does a syscall, or generates an addressing
//      or arithmetic exception.
//
//      For system calls, the following is the calling convention:
//
//      system call code -- r2
//              arg1 -- r4
//              arg2 -- r5
//              arg3 -- r6
//              arg4 -- r7
//
//      The result of the system call, if any, must be put back into r2.
//
// And don't forget to increment the pc before returning. (Or else you'll
// loop making the same system call forever!
//
//      "which" is the kind of exception.  The list of possible exceptions
//      are in machine.h.
//----------------------------------------------------------------------


char * ReadStringFromMachine(int from, unsigned max_size) {
  /* On copie octet par octet, de la mémoire user vers la mémoire noyau (buffer)
   * en faisant attention à bien convertir explicitement en char
   */
  int byte;
  unsigned int i;
  char * buffer = new char[max_size];
  for(i = 0; i < max_size-1; i++) {
    machine->ReadMem(from+i,1, &byte);
    if((char)byte=='\0')
      break;
    buffer[i] = (char) byte;
  }
  buffer[i] = '\0';
  return buffer;
}

void WriteStringToMachine(char * string, int to, unsigned max_size) {
  /* On copie octet par octet, en faisant attention à bien convertir
   * explicitement en char
   */
  char * bytes = (char *)(&machine->mainMemory[to]);
  for(unsigned int i = 0; i < max_size-1; i++) {
    bytes[i] = string[i];
    if(string[i]=='\0')
      break;
  }
}

typedef struct
{
    int f;
    int arg;
} UserThreadParameters;

static void StartUserThread(int parameters){
    UserThreadParameters *p = (UserThreadParameters *) parameters;
    currentThread->space->InitRegisters(p->f, p->arg);
    machine->Run();
}


void
ExceptionHandler (ExceptionType which)
{
    int type = machine->ReadRegister (2);

    if (which == SyscallException) {
        switch (type) {

        case SC_Halt: {
          DEBUG('a', "Shutdown, initiated by user program.\n");
          interrupt->Halt();
          break;
        }

        case SC_PutChar: {
          DEBUG('a', "PutChar, initiated by user program.\n");
          synchconsole->SynchPutChar((char)(machine->ReadRegister(4)));
          break;
        }

        case SC_PutString: {
          DEBUG('a', "PutString, initiated by user program.\n");
          // Le premier argument (registre R4) c'est l'adresse de la chaine de caractere
          // Que l'ont recopie dans le monde linux (noyau)
          // R4 >> pointeur vers la mémoire  MIPS
          // MAX_STRING_SIZE est defni prealablement dans code/threads/system.h
          char *buffer = ReadStringFromMachine(machine->ReadRegister(4), MAX_STRING_SIZE);
          synchconsole->SynchPutString(buffer);
          delete [] buffer;
          break;
        }

        case SC_GetChar: {
          DEBUG('a', "GetChar, initiated by user program.\n");
          machine->WriteRegister(2,(int) synchconsole->SynchGetChar());
          break;
        }

        case SC_GetString: {
          DEBUG('a', "GetString, initiated by user program.\n");

          // le premier argument est une adresse (char *)
          int to = machine->ReadRegister(4);
          // le second est un int >> la taille
          int size = machine->ReadRegister(5);
          // On donne pas acces à la mémoire directement, on ecrit ecrit dans un buffer
          // Peut etre pas obligé, mais au cas ou on utilise un buffer..
          char * buffer = new char[MAX_STRING_SIZE];
          synchconsole->SynchGetString(buffer, size);
          WriteStringToMachine(buffer, to, size);
          delete [] buffer;
          break;
        }

        case SC_PutInt: {
          DEBUG('a', "PutInt, initiated by user program.\n");
          // le premier est la valeur int
          int value = machine->ReadRegister(4);
          synchconsole->SynchPutInt(value);
          break;
        }

        case SC_UserThreadCreate:
        {
          DEBUG('a', "UserThreadCreate, initiated by user program.\n");

          int f = machine->ReadRegister(4);
          int arg = machine->ReadRegister(5);
          Thread * newThread = new Thread("Fonction f");

          UserThreadParameters *parameters = new UserThreadParameters;
          parameters->f = f;
          parameters->arg = arg;

          newThread->Fork(StartUserThread, (int) parameters);
          int ret = 0;
          if (newThread != NULL)
            ret = -1;

          machine->WriteRegister(2,ret);
          break;
        }

        case SC_GetInt: {
          DEBUG('a', "GetInt, initiated by user program.\n");
          int value = synchconsole->SynchGetInt();
          machine->WriteRegister(2, value);
          break;
        }

        case SC_Exit: {
          DEBUG('a', "Exit, initiated by user program.\n");
          interrupt->Halt();
          break;
        }

        default: {
          printf("Unexpected user mode exception %d %d\n", which, type);
          ASSERT(FALSE);
        }
      }
    }



    // LB: Do not forget to increment the pc before returning!
    UpdatePC ();
    // End of addition
}

