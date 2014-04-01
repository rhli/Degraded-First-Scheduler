== Welcome

This is CoRec native library. In this library we have corresponding code to support encode/decode operation for PM/JRS code.

* jrsencoder, jrs encoder for pipeline architecture
* jrsoencoder, jrs encoder for original architecture
* jrsdecoder, jrs decoder for pipeline architecture
* pmencoder, pm encoder for pipeline architecture
* pmoencoder, pm encoder for original architecture
* pmdecoder, pm decoder for pipeline architecture
* pminit, initialize encoding matrix for datanode pm encode
* pmrencode, pm encoder for datanode
* pmvalidate, pm validation for decoder to filter fail pattern

== Getting Started

1, testing

    bash test.sh

2, install
    
    bash install.sh

== To Add a New Coding Scheme ==
Suppose you want to add a new coding scheme called newCodingScheme.
1. Create a new class newCodingScheme which extends the Coding class.
2. You need to implement the following virtual functions:
    generate_encoding_matrix()
    test_validity()
    set_f2()
    encode2()
    encode_offline_recovery()
    reconstruct_lost_data()
    cleanup()
3. modify the makefile, re-make to generate a .so file.
    

== How to modified

1, Read the comment in the corresponding file

2, Modified corresponding test file in the test directory, make sure that the test can pass

3, Modified source file and make

