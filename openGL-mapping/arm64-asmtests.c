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
bool runInstrCheck(str16 strfun, void* addr, int imm_offset){

    int offs = 3;
    int blkSize = 256;
    // some test data that makes it easy to spot errors
    char* srcData = malloc(16);

    bool success = true;

    for(int i = 0; i < 16; i++){
        srcData[i]=i+1;
    }

    // to check that only what should be touched gets touched
    memset(addr, 0xff,blkSize);

    strfun(addr + offs, srcData, imm_offset);  // +3 to have it not aligned, +1 would work just as well, but this gives more space to spot overruns

    if(safe_memcmp(addr + offs + imm_offset, srcData, 16)){
        printf("Data mismatch!\n");
        printf("Source: \t");
        dump(srcData, 16);
        printf("\nDest: \t");
        dump(addr + offs + imm_offset, 16);
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

    if(memcheck(addr+offs + 16 + imm_offset, 0xff, blkSize - offs - 16 - imm_offset)){
        printf("Overrun in after the actual range! Data is: \n");
        dump(addr + 16 + offs, offs);
        printf("\n(should all be 0x%hhx)\n", 0xff);
        success = false;
    }

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
        "ldp x6, x7, [%1]\n\t"  // load the data into the registers
        "stp x6, x7, [x2]"  // the instruction actually being tested
        : : "r"(&dst), "r"(src) : "x2", "x6", "x7");
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
    uint64_t offs = 1;
    void* newdst = dst - 0x1; // to compensate for the offset
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
    uint64_t offs = 1;
    void* newdst = dst - 0x1; // to compensate the immediate offset
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

void runAsmTests(void* addr){

    runInstrCheck(strSIMD128unsignedImm, addr, 0);
    runInstrCheck(stp1,addr,0);
    runInstrCheck(stp2,addr,0);
    runInstrCheck(stp3,addr,0);
    runInstrCheck(stur1,addr,0);
    runInstrCheck(stur2,addr,0);
    runInstrCheck(str1,addr,0);
    runInstrCheck(str2,addr,0);
    runInstrCheck(str3,addr,0);
    runInstrCheck(str4,addr,0);
    runInstrCheck(str5,addr,0);
    runInstrCheck(str6,addr,0);

}