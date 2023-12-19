# SimpleOS: An Educational 32-bit Operating System

## Introduction
SimpleOS is a 32-bit educational operating system built from scratch using assembly and C. It utilizes shell scripts for disk writes and QEMU for system loading and debugging. This README provides an overview of its components and functionalities.

## Features

### System Boot and Loader
- Bootloader implementation with a two-stage loading process.

### Interrupt and Exception Handling
- Uses the IA-32 standard for managing interrupts and exceptions.

### Kernel Data Structures
- Basic kernel data structures library developed for internal operations.

### Process Scheduling and Resource Protection
- Time-slice process scheduling similar to Linux 0.1.1.
- Critical resource protection with mutex and semaphore implementations.

### Memory Management
- Virtual memory initiation with paging mechanism.
- Page table creation for individual processes.

### Security and Privilege Levels
- Simple priority levels with PL0 and PL3 for kernel and process isolation.

### System Calls and Standard Library Integration
- Implementation of system calls for process communication.
- Integration of the newlib library for standard functionalities.

### Console and Keyboard Input/Output
- Console input and output handling.
- Keyboard input processing.

### Device Management
- Development of a device management layer.
- Introduction of tty devices.

### Shell Interface
- Basic shell implementation supporting commands like echo, list, help, etc.

### File System and FAT16 Support
- File system development for tty device management.
- Implementation of the FAT16 file system with 'ls' command support.

## Getting Started

