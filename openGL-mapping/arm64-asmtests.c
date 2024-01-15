#include "arm64-asmtests.h"

int safe_memcmp(const void* s1, const void* s2, size_t n){
    volatile const char* p1 = s1;
    volatile const char* p2 = s2;
    for(size_t i = 0; i < n; i++){
        if(p1[i] < p2[i]) return -1;
        if(p1[i] > p2[i]) return 1;
    }
    return 0;
}

/* 
 * this just works for instructions that use offset registers to determine the address, 
 * but that's all I've seen and added fixups for so far, so that's fine.
 * 
 * I have a decent idea how I could somewhat decently test pre- and post-indexed instructions,
 * but I currently don't need those.
 * 
 * 
 * All of these functions take 16 bytes and store them at a given address with a given offset
 * 
 */


typedef void(*str16)(void* dst, void* src, int offset);

void dump(void* ptr, size_t n){
    for(int i = 0; i < n; i++){
        printf(" %hhx", ((unsigned char*)ptr)[i]);
    }
}

int memcheck(void* ptr, char c, size_t n){
    volatile const char* p1 = ptr;
    for(size_t i = 0; i < n; i++){
        if(p1[i] != c){
            return 1;
        }
    }
    return 0;
}

/*
 *
 * addr needs to point to a block at least 256 bytes large
 */
bool runInstrCheck(str16 strfun, void* addr, int imm_offset, int dataSize){

    int offs = 3;
    int blkSize = 256;
    // some test data that makes it easy to spot errors
    char* srcData = malloc(dataSize*2);

    bool success = true;

    memset(srcData, 0x55, dataSize*2);
    for(int i = 0; i < dataSize; i++){
        srcData[i]=i+1;
    }

    // to check that only what should be touched gets touched
    memset(addr, 0xff,blkSize);

    strfun(addr + offs, srcData, imm_offset);  // +3 to have it not aligned, +1 would work just as well, but this gives more space to spot overruns

    if(safe_memcmp(addr + offs + imm_offset, srcData, dataSize)){
        printf("Data mismatch!\n");
        printf("Source: \t");
        dump(srcData, dataSize);
        printf("\nDest: \t");
        dump(addr + offs + imm_offset, dataSize);
        printf("\n");
        success = false;
    }

    // the data has been copied correctly, but there could still be overruns
    if(memcheck(addr, 0xff, offs + imm_offset)){
        printf("Overrun in front of the actual address! Data is: \n");
        dump(addr, offs);
        printf("\n(should all be 0x%hhx)\n", 0xff);
        success = false;
    }

    if(memcheck(addr+offs + dataSize + imm_offset, 0xff, blkSize - offs - dataSize - imm_offset)){
        printf("Overrun in after the actual range! Data is: \n");
        dump(addr + dataSize + offs, offs);
        printf("\n(should all be 0x%hhx)\n", 0xff);
        success = false;
    }

    free(srcData);
    // all checks are done
    return success;
}


bool runLdrInstrCheck(str16 strfun, void* addr, int imm_offset, int dataSize){

    int offs = 3;
    int blkSize = 256;
    // some test data that makes it easy to spot errors
    char* dst = malloc(dataSize*4);

    char* srcData = addr + offs;

    bool success = true;

    // to check that only what should be touched gets touched
    memset(addr, 0xff,blkSize);
    memset(dst, 0xaa,dataSize*4);
    for(int i = 0; i < dataSize; i++){
        srcData[i]=i+1;
    }


    strfun(dst, srcData, imm_offset);  // +3 to have it not aligned, +1 would work just as well, but this gives more space to spot overruns

    if(safe_memcmp(dst, srcData, dataSize)){
        printf("Data mismatch!\n");
        printf("Source: \t");
        dump(srcData, dataSize);
        printf("\nDest: \t");
        dump(dst, dataSize);
        printf("\n");
        success = false;
    }

    // overruns don't make too much sense for this
    /*
    // the data has been copied correctly, but there could still be overruns
    if(memcheck(addr, 0xff, offs + imm_offset)){
        printf("Overrun in front of the actual address! Data is: \n");
        dump(addr, offs);
        printf("\n(should all be 0x%hhx)\n", 0xff);
        success = false;
    }

    if(memcheck(addr+offs + dataSize + imm_offset, 0xff, blkSize - offs - dataSize - imm_offset)){
        printf("Overrun in after the actual range! Data is: \n");
        dump(addr + dataSize + offs, offs);
        printf("\n(should all be 0x%hhx)\n", 0xff);
        success = false;
    }   
    */

    free(dst);
    // all checks are done
    return success;
}


// unfortunately, immediate offsets are kinda impossible to do as arguments
void strSIMD128unsignedImm(void* dst, void* src, int offset){

    uint64_t r0;
    __uint128_t tmp;
    printf("128bit SIMD/FP store\n");
    asm volatile("ldr %q0, [%1]\n\t"
                 "str %q0, [%2]" : "=w" (tmp) : "r" (src), "r" (dst));
    
}

/**
 * 
 * A lot of these are very similar, but since there seems to be maybe one instruction that isn't working
 * quite right, it could also be something that only happens with certain offsets, so I want to check all
 * instructions I've seen get fixed up
*/

// https://devblogs.microsoft.com/oldnewthing/20220726-00/?p=106898 may help with reg sizes (since I don't know aarch64 asm very much)


void ldrBehaviour1(void* dst, void* src, int offset){
    printf("64bit SIMD ldr (0xfd400021) behaviour test (data should be 1 2 3 4 5 6 7 8 0 0 0 0 0 0 0 0)\n");
    asm volatile(
        "ldr x1, [%1]\n\t"  // src
        "ldr q1, [x1]\n\t"
        "ldr d1, [x1]\n\t"
        "str q1, [%0]\n\t"
        : : "r"(dst), "r"(&src) : "x1", "q1", "d1");
}

void ldrBehaviour2(void* dst, void* src, int offset){
    printf("32bit ldr behaviour test (data should show 1 2 3 4 0 0 0 0 aa aa aa aa aa aa aa aa)\n");
    asm volatile(
        "ldr x1, [%1]\n\t"  // src
        "ldr x2, [x1]\n\t"
        "ldr w2, [x1]\n\t"
        "str x2, [%0]\n\t"
        : : "r"(dst), "r"(&src) : "x1", "x2", "w2");
}

// 0xf840816a
void ldur1(void* dst, void* src, int offset){
    printf("64bit ldur (0xf840816a)\n");
    void* newsrc = src - 0x8; // to compensate the immediate offset
    asm volatile(
        "ldr x11, [%1]\n\t"  // src
        "ldur x10, [x11, #8]\n\t"
        "str x10, [%0]\n\t"
        : : "r"(dst), "r"(&newsrc) : "x11", "x10");
}

// 0xfc408021
void ldur2(void* dst, void* src, int offset){
    printf("64bit SIMD ldur (0xfc408021)\n");
    void* newsrc = src - 0x8; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%1]\n\t"  // src
        "ldur d1, [x1, #8]\n\t"
        "str d1, [%0]\n\t"
        : : "r"(dst), "r"(&newsrc) : "x1", "d1");
}

// f85f816a
void ldur3(void* dst, void* src, int offset){
    printf("64bit ldur (0xf85f816a)\n");
    void* newsrc = src + 0x8; // to compensate the immediate offset
    asm volatile(
        "ldr x11, [%1]\n\t"  // src
        "ldur x10, [x11, #-8]\n\t"
        "str x10, [%0]\n\t"
        : : "r"(dst), "r"(&newsrc) : "x11", "x10");
}

// 0xfd400021
void ldr1(void* dst, void* src, int offset){
    printf("64bit SIMD ldr (0xfd400021)\n");
    asm volatile(
        "ldr x1, [%1]\n\t"  // src
        "ldr d1, [x1]\n\t"
        "str d1, [%0]\n\t"
        : : "r"(dst), "r"(&src) : "x1", "d1");
}

// f940016a
void ldr2(void* dst, void* src, int offset){
    printf("64bit ldr (0xf940016a)\n");
    asm volatile(
        "ldr x11, [%1]\n\t"  // src
        "ldr x10, [x11]\n\t"
        "str x10, [%0]\n\t"
        : : "r"(dst), "r"(&src) : "x10", "x11");
}

// a9000c22
void stp1(void* dst, void* src, int offset){
    printf("64bit stp (0xa9000c22)\n");
    asm volatile(
        "ldr x1, [%0]\n\t"  // dst address into x1
        "ldp x2, x3, [%1]\n\t"  // load the data into the registers
        "stp x2, x3, [x1]"
        : : "r"(&dst), "r"(src) : "x2", "x3", "x1");
}

// a9001444
void stp2(void* dst, void* src, int offset){
    printf("64bit stp (0xa9001444)\n");
    asm volatile(
        "ldr x2, [%0]\n\t"  // dst address into x1
        "ldp x4, x5, [%1]\n\t"  // load the data into the registers
        "stp x4, x5, [x2]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x2", "x4", "x5");
}

// a9001c46
void stp3(void* dst, void* src, int offset){
    printf("64bit stp (0xa9001c46)\n");
    asm volatile(
        "ldr x2, [%0]\n\t"  // dst address into x1
        "ldp x6, x7, [%1]\n\t"  // load the data into the registers
        "stp x6, x7, [x2]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x2", "x6", "x7");
}

// ad000440
void stp4(void* dst, void* src, int offset){
    printf("128bit SIMD stp (0xad000440) total 256bit\n");
    asm volatile(
        "ldr x2, [%0]\n\t"  // dst address into x1
        "ldp q0, q1, [%1]\n\t"  // load the data into the registers
        "stp q0, q1, [x2]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x2", "q0", "q1");
}

// 3c8041a0
void stur1(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c8041a0)\n");
    void* newdst = dst - 4; // to compensate the immediate offset
    asm volatile(
        "ldr x13, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "stur q0, [x13, #4]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x13", "q0");
}

// 3c80c0e0
void stur2(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c80c0e0)\n");
    void* newdst = dst - 0xc; // to compensate the immediate offset
    asm volatile(
        "ldr x7, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "stur q0, [x7, #0xc]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x7", "q0");
}

// 3c810140
void stur3(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c810140)\n");
    void* newdst = dst - 0x10; // to compensate the immediate offset
    asm volatile(
        "ldr x10, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "stur q0, [x10, #0x10]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x10", "q0");
}

// 3c820141
void stur4(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c820141)\n");
    void* newdst = dst - 0x20; // to compensate the immediate offset
    asm volatile(
        "ldr x10, [%0]\n\t"  // newdst address into x13
        "ldr q1, [%1]\n\t"  // load the data into the register
        "stur q1, [x10, #0x20]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x10", "q1");
}

// 3c830142
void stur5(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c830142)\n");
    void* newdst = dst - 0x30; // to compensate the immediate offset
    asm volatile(
        "ldr x10, [%0]\n\t"  // newdst address into x13
        "ldr q2, [%1]\n\t"  // load the data into the register
        "stur q2, [x10, #0x30]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x10", "q2");
}

// 3c840143
void stur6(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c840143)\n");
    void* newdst = dst - 0x40; // to compensate the immediate offset
    asm volatile(
        "ldr x10, [%0]\n\t"  // newdst address into x13
        "ldr q3, [%1]\n\t"  // load the data into the register
        "stur q3, [x10, #0x40]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x10", "q3");
}

// 3c810022
void stur7(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c810022)\n");
    void* newdst = dst - 0x10; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q2, [%1]\n\t"  // load the data into the register
        "stur q2, [x1, #0x10]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q2");
}

// 3c820021
void stur8(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c820021)\n");
    void* newdst = dst - 0x20; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q1, [%1]\n\t"  // load the data into the register
        "stur q1, [x1, #0x20]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q1");
}

// 3c830022
void stur9(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c830022)\n");
    void* newdst = dst - 0x30; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q2, [%1]\n\t"  // load the data into the register
        "stur q2, [x1, #0x30]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q2");
}

// 3c80c026
void stur10(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c80c026)\n");
    void* newdst = dst - 0xc; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q6, [%1]\n\t"  // load the data into the register
        "stur q6, [x1, #0xc]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q6");
}

// 3c818027
void stur11(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c818027)\n");
    void* newdst = dst - 0x18; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q7, [%1]\n\t"  // load the data into the register
        "stur q7, [x1, #0x18]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q7");
} 

// 3c810021
void stur12(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c810021)\n");
    void* newdst = dst - 0x10; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q1, [%1]\n\t"  // load the data into the register
        "stur q1, [x1, #0x10]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q1");
} 

// 3c80c020
void stur13(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c80c020)\n");
    void* newdst = dst - 0xc; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "stur q0, [x1, #0xc]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q0");
}

// 3c818025
void stur14(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c818025)\n");
    void* newdst = dst - 0x18; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q5, [%1]\n\t"  // load the data into the register
        "stur q5, [x1, #0x18]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q5");
}

// 3c80c024
void stur15(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c80c024)\n");
    void* newdst = dst - 0xc; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q4, [%1]\n\t"  // load the data into the register
        "stur q4, [x1, #0xc]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q4");
}

// 3c818026
void stur16(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c80c024)\n");
    void* newdst = dst - 0x18; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q6, [%1]\n\t"  // load the data into the register
        "stur q6, [x1, #0x18]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q6");
}

// 3c820020
void stur17(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c820020)\n");
    void* newdst = dst - 0x20; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "stur q0, [x1, #0x20]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q0");
}

// 3c830021
void stur18(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c830021)\n");
    void* newdst = dst - 0x30; // to compensate the immediate offset
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q1, [%1]\n\t"  // load the data into the register
        "stur q1, [x1, #0x30]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x1", "q1");
}

// f81f81aa
void stur19(void* dst, void* src, int offset){
    printf("64bit stur (0xf81f81aa)\n");
    void* newdst = dst + 0x8; // to compensate the immediate offset
    asm volatile(
        "ldr x13, [%0]\n\t"  // newdst address into x13
        "ldr x10, [%1]\n\t"  // load the data into the register
        "stur x10, [x13, #-8]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x10", "x13");
}

// fc0081a1
void stur20(void* dst, void* src, int offset){
    printf("64bit SIMD stur (0xfc0081a1)\n");
    void* newdst = dst - 0x8; // to compensate the immediate offset
    asm volatile(
        "ldr x13, [%0]\n\t"  // newdst address into x13
        "ldr d1, [%1]\n\t"  // load the data into the register
        "stur d1, [x13, #8]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "d1", "x13");
}

// 3c9c01a0
void stur21(void* dst, void* src, int offset){
    printf("128bit SIMD stur (0x3c9c01a0)\n");
    void* newdst = dst + 0x40; // to compensate the immediate offset
    asm volatile(
        "ldr x13, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "stur q0, [x13, #-0x40]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x13", "q0");
}

// 3d8002e0
void str1(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3d8002e0)\n");
    asm volatile(
        "ldr x23, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "str q0, [x23]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x23", "q0");
}

// f9000845
void str2(void* dst, void* src, int offset){
    printf("64bit str (0xf9000845)\n");
    void* newdst = dst - 0x10; // to compensate the immediate offset
    asm volatile(
        "ldr x2, [%0]\n\t"  // newdst address into x13
        "ldr x5, [%1]\n\t"  // load the data into the register
        "str x5, [x2, #0x10]\n\t"  // the instruction actually being tested

        "ldr x5, [%1, #0x08]\n\t"  // load the second half
        "add x2, x2, #0x08\n\t"    // new dst address
        "str x5, [x2, #0x10]\n\t"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src) : "x5", "x2");
}

// 3ca26861
void str3(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3ca26861)\n");
    int64_t offs = -2;
    void* newdst = dst + 0x2; // to compensate for the offset
    asm volatile(
        "ldr x3, [%0]\n\t"  // newdst address into x13
        "ldr x2, [%2]\n\t"
        "ldr q1, [%1]\n\t"  // load the data into the register
        "str q1, [x3, x2]"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src), "r"(&offs) : "x2", "q1", "x3");
}

// 3d800140
void str4(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3d800140)\n");
    asm volatile(
        "ldr x10, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "str q0, [x10]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x10", "q0");
}

// b9000051
void str5(void* dst, void* src, int offset){
    printf("32bit str (0xb9000051)\n");
    asm volatile(
        "ldr x2, [%0]\n\t"  // newdst address into x13
        "ldr w17, [%1]\n\t"  // load the data into the register
        "str w17, [x2]\n\t"  // the instruction actually being tested

        "ldr w17, [%1, #0x04]\n\t"
        "add x2, x2, #0x04\n\t"
        "str w17, [x2]\n\t"  // the instruction actually being tested

        "ldr w17, [%1, #0x08]\n\t"
        "add x2, x2, #0x04\n\t"
        "str w17, [x2]\n\t"  // the instruction actually being tested

        "ldr w17, [%1, #0x0c]\n\t"
        "add x2, x2, #0x04\n\t"
        "str w17, [x2]\n\t"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "w17", "x2");
}

// f82468a2
void str6(void* dst, void* src, int offset){
    printf("64bit str (0xf82468a2)\n");
    int64_t offs = -2;
    void* newdst = dst + 0x2; // to compensate the immediate offset
    asm volatile(
        "ldr x5, [%0]\n\t"  // newdst address into x13
        "ldr x4, [%2]\n\t"
        "ldr x2, [%1]\n\t"  // load the data into the register
        "str x2, [x5, x4]\n\t"  // the instruction actually being tested

        "ldr x2, [%1, #0x08]\n\t"  // load the second half
        "add x5, x5, #0x08\n\t"    // new dst address
        "str x2, [x5, x4]\n\t"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src), "r"(&offs) : "x5", "x2", "x4");
}

// 3d800021
void str7(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3d800021)\n");
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q1, [%1]\n\t"  // load the data into the register
        "str q1, [x1]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x1", "q1");
}

// 3d800022
void str8(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3d800022)\n");
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q2, [%1]\n\t"  // load the data into the register
        "str q2, [x1]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x1", "q2");
}

// 3d800020
void str9(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3d800020)\n");
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q0, [%1]\n\t"  // load the data into the register
        "str q0, [x1]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x1", "q0");
}

// 3d800024
void str10(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3d800024)\n");
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q4, [%1]\n\t"  // load the data into the register
        "str q4, [x1]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x1", "q4");
}

// 3d800025
void str11(void* dst, void* src, int offset){
    printf("128bit SIMD str (0x3d800025)\n");
    asm volatile(
        "ldr x1, [%0]\n\t"  // newdst address into x13
        "ldr q5, [%1]\n\t"  // load the data into the register
        "str q5, [x1]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x1", "q5");
}

// f8226865
void str12(void* dst, void* src, int offset){
    printf("64bit str (0xf8226865)\n");
    int64_t offs = -2;
    void* newdst = dst + 0x2; // to compensate the immediate offset
    asm volatile(
        "ldr x3, [%0]\n\t"  // newdst address into x13
        "ldr x2, [%2]\n\t"
        "ldr x5, [%1]\n\t"  // load the data into the register
        "str x5, [x3, x2]\n\t"  // the instruction actually being tested

        "ldr x5, [%1, #0x08]\n\t"  // load the second half
        "add x3, x3, #0x08\n\t"    // new dst address
        "str x5, [x3, x2]\n\t"  // the instruction actually being tested
        : : "r"(&newdst), "r"(src), "r"(&offs) : "x5", "x2", "x3");
}

// fd0001a1
void str13(void* dst, void* src, int offset){
    printf("64bit SIMD str (0xfd0001a1)\n");
    asm volatile(
        "ldr x13, [%0]\n\t"  // newdst address into x13
        "ldr d1, [%1]\n\t"  // load the data into the register
        "str d1, [x13]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x13", "d1");
}

void runAsmTests(void* addr){
    printf("\nstore instructions: \n");
    runInstrCheck(strSIMD128unsignedImm, addr, 0, 16);
    runInstrCheck(stp1,addr,0, 16);
    runInstrCheck(stp2,addr,0, 16);
    runInstrCheck(stp3,addr,0, 16);
    runInstrCheck(stp4,addr,0, 32);
    runInstrCheck(stur1,addr,0, 16);
    runInstrCheck(stur2,addr,0, 16);
    runInstrCheck(stur3,addr,0, 16);
    runInstrCheck(stur4,addr,0, 16);
    runInstrCheck(stur5,addr,0, 16);
    runInstrCheck(stur6,addr,0, 16);
    runInstrCheck(stur7,addr,0, 16);
    runInstrCheck(stur8,addr,0, 16);
    runInstrCheck(stur9,addr,0, 16);
    runInstrCheck(stur10,addr,0, 16);
    runInstrCheck(stur11,addr,0, 16);
    runInstrCheck(stur12,addr,0, 16);
    runInstrCheck(stur13,addr,0, 16);
    runInstrCheck(stur14,addr,0, 16);
    runInstrCheck(stur15,addr,0, 16);
    runInstrCheck(stur16,addr,0, 16);
    runInstrCheck(stur17,addr,0, 16);
    runInstrCheck(stur18,addr,0, 16);
    runInstrCheck(stur19,addr,0, 8);
    runInstrCheck(stur20,addr,0, 8);
    runInstrCheck(stur21,addr,0, 16);
    runInstrCheck(str1,addr,0, 16);
    runInstrCheck(str2,addr,0, 16);
    runInstrCheck(str3,addr,0, 16);
    runInstrCheck(str4,addr,0, 16);
    runInstrCheck(str5,addr,0, 16);
    runInstrCheck(str6,addr,0, 16);
    runInstrCheck(str7,addr,0, 16);
    runInstrCheck(str8,addr,0, 16);
    runInstrCheck(str9,addr,0, 16);
    runInstrCheck(str10,addr,0, 16);
    runInstrCheck(str11,addr,0, 16);
    runInstrCheck(str12,addr,0, 16);
    runInstrCheck(str13,addr,0, 8);

    printf("\nload instructions:\n");
    runLdrInstrCheck(ldur1,addr,0, 8);
    runLdrInstrCheck(ldur2,addr,0, 8);
    runLdrInstrCheck(ldur3,addr,0, 8);
    runLdrInstrCheck(ldr1,addr,0, 8);
    runLdrInstrCheck(ldr2,addr,0, 8);

    runLdrInstrCheck(ldrBehaviour1,addr,0, 16);
    runLdrInstrCheck(ldrBehaviour2,addr,0, 16);


}