# op-sys-p5
Simple Distributed File Server

Your file server is built as a stand-alone UDP-based server. It should wait for a message and then process the message as need be, replying to the given client.

Your file server will store all of its data in an on-disk file which will be referred to as the file system image . Your on-disk file system structures should roughly follow that of the log-structured file system discussed in class. This image contains the on-disk representation of your data structures; you should use these system calls to access it: open(), read(), write(), lseek(), close(), fsync() .

To access the file server, you will be building a client library. The interface that the library supports is defined in mfs.h . The library should be called libmfs.so , and any programs that wish to access your file server will link with it and call its various routines.

http://pages.cs.wisc.edu/~remzi/Classes/537/Fall2013/Projects/p5a.html
