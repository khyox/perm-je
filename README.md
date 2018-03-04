# PERsistent heap Management library

### Overview 

[PERM](https://computation.llnl.gov/projects/memory-centric-architectures/perm)
  is a 'C' library for persistent heap management and is intended for 
  use with a dynamic-memory allocator (e.g. malloc, free). The PERM memory 
  allocator replaces the standard 'C' dynamic memory allocation functions 
  with compatible versions that provide persistent memory to application 
  programs. 

Memory allocated with the PERM allocator will persist between 
  program invocations after a call to a checkpoint function. This function 
  essentially saves the state of the heap and registered global variables 
  to a file which may reside in flash memory or other node local storage. 

A few other functions are also provided by the library to manage 
  checkpoint files. Global variables in an application can be marked 
  persistent and be included in a checkpoint by using a compiler attribute 
  defined as PERM. The PERM checkpoint method is not dependent on the 
  programming model and works with distributed memory or shared memory 
  programs. 

### Method

PERM works closely with the dynamic memory allocator and monitors chunks 
  of memory returned by the operating system and used by the application 
  program. Whole chunks or portions thereof are written to persistent 
  media at checkpoint time. 

### Platform support

Any 32-bit or 64-bit machine with Linux, FreeBSD, or Mac OS X is 
  supported. 

Linux, FreeBSD, Mac OS X (mmap support is required) 

### Details

Please, read ``README.PERM`` for further details.

### Tuning the kernel

For PERM to work in the right conditions, some kernel tunning is advisable:

* Turn off periodic flush to file and dirty ratio flush:
```
echo 0 > /proc/sys/vm/dirty_writeback_centisecs
echo 100 > /proc/sys/vm/dirty_background_ratio
echo 100 > /proc/sys/vm/dirty_ratio
```
* Turn off address space randomization:
```
echo 0 > /proc/sys/kernel/randomize_va_space 
```

### References

1. PERM, https://computation.llnl.gov/projects/memory-centric-architectures/perm
2. jemalloc, dynamic memory allocator, https://github.com/jemalloc/jemalloc 
