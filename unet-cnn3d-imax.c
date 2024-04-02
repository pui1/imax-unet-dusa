
static char RcsHeader[] = "$Header: /usr/home/nakashim/proj-arm64/sample/mm_cnn_lf/RCS/cnn.c,v 1.4 2018/02/04 10:28:43 nakashim Exp nakashim $";

/*                          Copyright (C) 2013- by NAIST */
/*                           Primary writer: Y.Nakashima */
/*                                  nakashim@is.naist.jp */

#ifndef UTYPEDEF
#define UTYPEDEF
typedef unsigned char      Uchar;
typedef unsigned short     Ushort;
typedef unsigned int       Uint;
typedef unsigned long long Ull;
typedef long long int      Sll;
#if __AARCH64EL__ == 1
typedef long double Dll;
#else
typedef struct {Ull u[2];} Dll;
#endif
#endif

#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <fcntl.h>
#include <math.h>
#ifndef ARMSIML
#include <unistd.h>
#include <sys/times.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <pthread.h>
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/extensions/Xdbe.h>
#endif

int WD=320, HT=320, BITMAP=320*320, SCRWD=5, SCRHT=4, VECWD=240, VECHT=240, VECSTEP=4;

#if defined(EMAX6)
#include "../../src/conv-c2c/emax6.h"
#include "../../src/conv-c2c/emax6lib.c"
#endif
#if defined(EMAX7)
#include "../../src/conv-c2d/emax7.h"
#include "../../src/conv-c2d/emax7lib.c"
#endif
#if !defined(ARMSIML)
#include "./xdisp.c"
#endif

Uchar* membase;

sysinit(memsize, alignment) Uint memsize, alignment;
{
#if defined(EMAX5) && defined(ARMZYNQ)
  if (emax5_open() == NULL)
    exit(1);
#elif defined(EMAX6) && defined(ARMZYNQ)
  if (emax6_open() == NULL)
    exit(1);
#elif defined(EMAX7)
#if defined(ARMZYNQ)
  if ((NLANE = emax7_open()) == NULL)
    exit(1);
#else
  NLANE = 1;
#endif
#endif

#if defined(EMAX5) && defined(ARMZYNQ)
  membase = emax_info.hpp_mmap;
  {volatile int i; for (i=0; i<(memsize+sizeof(Dll)-1)/sizeof(Dll); i++) *((Dll*)membase+i)=0;}
#elif defined(EMAX6) && defined(ARMZYNQ)
  membase = emax_info.ddr_mmap;
  {volatile int i; for (i=0; i<(memsize+sizeof(Dll)-1)/sizeof(Dll); i++) *((Dll*)membase+i)=0;}
#elif defined(EMAX7) && defined(ARMZYNQ)
  membase = emax_info[0].ddr_mmap;
  {volatile int i; for (i=0; i<(memsize+sizeof(Dll)-1)/sizeof(Dll); i++) *((Dll*)membase+i)=0;}
#elif __linux__ == 1
  posix_memalign(&membase, alignment, memsize);
#else
  membase = (void*)malloc(memsize+alignment);
  if ((int)membase & (alignment-1))
    membase = (void*)(((int)membase & ~(alignment-1))+alignment);
#endif

#if defined(EMAX5)
#if !defined(ARMZYNQ)
  emax_info.hpp_phys = membase;
  emax_info.hpp_mmap = emax_info.hpp_phys;
  emax_info.acp_phys = ACP_BASE2_PHYS; /* defined in emax5lib.h >= ALOCLIMIT */
  emax_info.acp_mmap = emax_info.acp_phys;
#endif
  acp_conf = emax_info.acp_mmap; /* 8KB * 256sets */
  acp_lmmi = emax_info.acp_mmap + 0x200000;
  acp_regv = emax_info.acp_mmap + 0x304000;
#endif

#if defined(EMAX6)
#if !defined(ARMZYNQ)
  emax_info.dma_phys = DMA_BASE2_PHYS; /* defined in emax6lib.h */
  emax_info.dma_mmap = emax_info.dma_phys;
  emax_info.reg_phys = REG_BASE2_PHYS; /* defined in emax6lib.h */
  emax_info.reg_mmap = emax_info.reg_phys;
  emax_info.lmm_phys = LMM_BASE2_PHYS;
  emax_info.lmm_mmap = emax_info.lmm_phys;
  emax_info.ddr_phys = membase;
  emax_info.ddr_mmap = emax_info.ddr_phys;
#endif
#if defined(ARMSIML) || defined(ARMZYNQ)
  emax6.dma_ctrl  = emax_info.dma_mmap;
  emax6.reg_ctrl  = emax_info.reg_mmap;
  ((struct reg_ctrl*)emax6.reg_ctrl)->i[0].cmd = CMD_RESET;  // ������ RESET
#if defined(ARMZYNQ)
  usleep(1);
#endif
  ((struct reg_ctrl*)emax6.reg_ctrl)->i[0].adtr = emax_info.ddr_mmap - emax_info.lmm_phys;
  ((struct reg_ctrl*)emax6.reg_ctrl)->i[0].dmrp = 0LL;
  switch (((struct reg_ctrl*)emax6.reg_ctrl)->i[0].stat>>8 & 0xf) {
  case  3:EMAX_DEPTH = 64;break;
  case  2:EMAX_DEPTH = 32;break;
  case  1:EMAX_DEPTH = 16;break;
  case  0:EMAX_DEPTH =  8;break;
  default:
    printf("sysinit: illegal stat=%x for setting EMAX_DEPTH\n",((struct reg_ctrl*)emax6.reg_ctrl)->i[0].stat>>8 & 0xf);
    exit(1);
  }
  printf("EMAX6.DEPTH=%d\n", EMAX_DEPTH);
#endif
#endif

#if defined(EMAX7) && (defined(ARMSIML) || defined(ARMZYNQ))
  { int i;
    for (i=0; i<NLANE; i++) {
      emax7[i].dma_ctrl  = emax_info[i].dma_mmap;
      emax7[i].reg_ctrl  = emax_info[i].reg_mmap;
      ((struct reg_ctrl*)emax7[i].reg_ctrl)->i[0].cmd = CMD_RESET;  // ������ RESET
#if defined(ARMZYNQ)
      usleep(1);
#endif
      ((struct reg_ctrl*)emax7[i].reg_ctrl)->i[0].adtr = emax_info[i].ddr_mmap - emax_info[i].lmm_phys;
      ((struct reg_ctrl*)emax7[i].reg_ctrl)->i[0].dmrp = 0LL;
      switch (((struct reg_ctrl*)emax7[0].reg_ctrl)->i[0].stat>>8 & 0xf) {
      case  3:EMAX_DEPTH = 64;break;
      case  2:EMAX_DEPTH = 32;break;
      case  1:EMAX_DEPTH = 16;break;
      case  0:EMAX_DEPTH =  8;break;
      default:
	printf("init_xmax: illegal stat=%x for setting EMAX_DEPTH\n",((struct reg_ctrl*)emax7[i].reg_ctrl)->i[0].stat>>8 & 0xf);
	exit(1);
      }
      printf("EMAX7[%d].DEPTH=%d\n", i, EMAX_DEPTH);
    }
  }
  printf("EMAX7: NLANE=%d DEPTH=%d\n", NLANE, EMAX_DEPTH);
#endif
}

// #define IC    1024
// //IMAP should be specified in Makefile* to share src for emax6/emax7.
// //#define IMAP  6  for 64unit
// //#define IMAP  3  for 32unit
// // #define IMAP  2 //////////

// #define OC    1024
// #define D     6    // 20
// #define H     18   // 269 + 3
// #define W     16   // 244
// #define RMGRP 2  // (64)
// //NCHIP should be specified in Makefile* to share src for emax6/emax7.
// //#define NCHIP 4 for emax6
// //#define NCHIP 1 for emax7

// #define K     3
// #define W_    4

#ifndef CONV_SELECT
#define CONV_SELECT 1
#endif

#if CONV_SELECT == 1
#define IN_PATH "cnn3d_bins/befo_conv1.bin"
#define KER_PATH "cnn3d_bins/conv1_weight.bin"
#define OUT_PATH "imax_out/imax_conv1.bin"
#define IC 1
#define OC 64
#define D 20
#define H 269
#define W 244
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 2
#define IN_PATH "cnn3d_bins/befo_conv2.bin"
#define KER_PATH "cnn3d_bins/conv2_weight.bin"
#define OUT_PATH "imax_out/imax_conv2.bin"
#define IC 64
#define OC 64
#define D 20
#define H 269
#define W 244
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 3
#define IN_PATH "cnn3d_bins/befo_conv3.bin"
#define KER_PATH "cnn3d_bins/conv3_weight.bin"
#define OUT_PATH "imax_out/imax_conv3.bin"
#define IC 64
#define OC 128
#define D 20
#define H 135
#define W 122
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 4
#define IN_PATH "cnn3d_bins/befo_conv4.bin"
#define KER_PATH "cnn3d_bins/conv4_weight.bin"
#define OUT_PATH "imax_out/imax_conv4.bin"
#define IC 128
#define OC 128
#define D 20
#define H 135
#define W 122
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 5
#define IN_PATH "cnn3d_bins/befo_conv5.bin"
#define KER_PATH "cnn3d_bins/conv5_weight.bin"
#define OUT_PATH "imax_out/imax_conv5.bin"
#define IC 128
#define OC 256
#define D 10
#define H 68
#define W 61
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 6
#define IN_PATH "cnn3d_bins/befo_conv6.bin"
#define KER_PATH "cnn3d_bins/conv6_weight.bin"
#define OUT_PATH "imax_out/imax_conv6.bin"
#define IC 256
#define OC 256
#define D 10
#define H 68
#define W 61
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 7
#define IN_PATH "cnn3d_bins/befo_conv7.bin"
#define KER_PATH "cnn3d_bins/conv7_weight.bin"
#define OUT_PATH "imax_out/imax_conv7.bin"
#define IC 256
#define OC 512
#define D 5
#define H 34
#define W 31
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 8
#define IN_PATH "cnn3d_bins/befo_conv8.bin"
#define KER_PATH "cnn3d_bins/conv8_weight.bin"
#define OUT_PATH "imax_out/imax_conv8.bin"
#define IC 512
#define OC 512
#define D 5
#define H 34
#define W 31
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 9
#define IN_PATH "cnn3d_bins/befo_conv9.bin"
#define KER_PATH "cnn3d_bins/conv9_weight.bin"
#define OUT_PATH "imax_out/imax_conv9.bin"
#define IC 512
#define OC 1024
#define D 5
#define H 17
#define W 16
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 10
#define IN_PATH "cnn3d_bins/befo_conv10.bin"
#define KER_PATH "cnn3d_bins/conv10_weight.bin"
#define OUT_PATH "imax_out/imax_conv10.bin"
#define IC 1024
#define OC 1024
#define D 5
#define H 17
#define W 16
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 11
#define IN_PATH "cnn3d_bins/befo_conv11.bin"
#define KER_PATH "cnn3d_bins/conv11_weight.bin"
#define OUT_PATH "imax_out/imax_conv11.bin"
#define IC 1024
#define OC 512
#define D 5
#define H 34
#define W 31
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 12
#define IN_PATH "cnn3d_bins/befo_conv12.bin"
#define KER_PATH "cnn3d_bins/conv12_weight.bin"
#define OUT_PATH "imax_out/imax_conv12.bin"
#define IC 512
#define OC 512
#define D 5
#define H 34
#define W 31
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 13
#define IN_PATH "cnn3d_bins/befo_conv13.bin"
#define KER_PATH "cnn3d_bins/conv13_weight.bin"
#define OUT_PATH "imax_out/imax_conv13.bin"
#define IC 512
#define OC 256
#define D 10
#define H 68
#define W 61
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 14
#define IN_PATH "cnn3d_bins/befo_conv14.bin"
#define KER_PATH "cnn3d_bins/conv14_weight.bin"
#define OUT_PATH "imax_out/imax_conv14.bin"
#define IC 256
#define OC 256
#define D 10
#define H 68
#define W 61
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 15
#define IN_PATH "cnn3d_bins/befo_conv15.bin"
#define KER_PATH "cnn3d_bins/conv15_weight.bin"
#define OUT_PATH "imax_out/imax_conv15.bin"
#define IC 256
#define OC 128
#define D 20
#define H 135
#define W 122
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 16
#define IN_PATH "cnn3d_bins/befo_conv16.bin"
#define KER_PATH "cnn3d_bins/conv16_weight.bin"
#define OUT_PATH "imax_out/imax_conv16.bin"
#define IC 128
#define OC 128
#define D 20
#define H 135
#define W 122
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 17
#define IN_PATH "cnn3d_bins/befo_conv17.bin"
#define KER_PATH "cnn3d_bins/conv17_weight.bin"
#define OUT_PATH "imax_out/imax_conv17.bin"
#define IC 128
#define OC 64
#define D 20
#define H 269
#define W 244
#define RMGRP 2
#define K 3
#define W_ 4
#endif

#if CONV_SELECT == 18
#define IN_PATH "cnn3d_bins/befo_conv18.bin"
#define KER_PATH "cnn3d_bins/conv18_weight.bin"
#define OUT_PATH "imax_out/imax_conv18.bin"
#define IC 64
#define OC 64
#define D 20
#define H 269
#define W 244
#define RMGRP 2
#define K 3
#define W_ 4
#endif





Uint *in;  /*[IC*D*H*W];*/
Uint *ker; /*[IC*OC*K*K*K];*/
Uint *out0;/*[OC*D*H*W];*/
Uint *out1;/*[OC*D*H*W];*/
Uint *ip0, *ip1, *ip2, *ip3, *ip4, *ip5, *kp, *op;
int ic, dep, row, col, oc, z, y, x;
int ztop, top, iset;
int w, kidx;
int count0, count1, count2;

#define CSIMWD 320
#define CSIMHT 240
#define CSIMBM (CSIMWD*CSIMHT)
Uint Z[CSIMBM];

#define MAXINT (~(1<<(sizeof(int)*8-1)))
#define adif(a,b) (((a)>(b))?(a)-(b):(b)-(a))
#define dif(a,b)  (adif((((a)>>24)&255), (((b)>>24)&255))\
                  +adif((((a)>>16)&255), (((b)>>16)&255))\
                  +adif((((a)>> 8)&255), (((b)>> 8)&255)))
#define abs(a) (((a)<0)?-(a):(a))

main()
{
  printf("CONV_SELECT= %d\n", CONV_SELECT);
  printf("check1 IC %d OC %d D %d H %d W %d K %d\n", IC, OC, D, H, W, K);
  printf("check2 RMGRP %d NCHIP %d IMAP %d\n", RMGRP, NCHIP, IMAP);
  sysinit(IC*D*H*W*sizeof(int)
         +IC*OC*K*K*K*sizeof(int)
         +OC*D*H*W*sizeof(int)
         +OC*D*H*W*sizeof(int),32);
  printf("membase: %08.8x\n", (Uint)membase);
  in   = (Uint*)membase;
  ker  = (Uint*)((Uchar*)in   + IC*D*H*W*sizeof(int));
  out0 = (Uint*)((Uchar*)ker  + IC*OC*K*K*K*sizeof(int));
  out1 = (Uint*)((Uchar*)out0 + OC*D*H*W*sizeof(int));
  printf("in  : %08.8x\n", in);
  printf("ker : %08.8x\n", ker);
  printf("out0: %08.8x\n", out0);
  printf("out1: %08.8x\n", out1);

  // for (ic=0; ic<IC; ic++) {
  //   for (dep=0; dep<D; dep++) {
  //     for (row=0; row<H; row++) {
	// for (col=0; col<W; col++) {
	//   // *(float*)&in[ic*D*H*W+dep*H*W+row*W+col] = ic<<12|(((D/2-abs(dep-D/2))*(H/2-abs(row-H/2))*(W/2-abs(col-W/2)))&0xfff);
  //   *(float*)&in[ic*D*H*W+dep*H*W+row*W+col] = 1; 
	// }
  //     }
  //   }
  // }
  // for (oc=0; oc<OC; oc++) {
  //   for (ic=0; ic<IC; ic++) {
  //     for (z=0; z<K; z++) {
	// for (y=0; y<K; y++) {
	//   for (x=0; x<K; x++) {
	//     *(float*)&ker[ic*OC*K*K*K+oc*K*K*K+z*K*K+y*K+x] = (oc-ic)*((2-abs(z-K/2))*(2-abs(y-K/2))*(2-abs(x-K/2)))/OC;
  //     // *(float*)&ker[ic*OC*K*K*K+oc*K*K*K+z*K*K+y*K+x] = 1;
	//   }
	// }
  //     }
  //   }
  // }

    // ---------------------------------------------------------
    // load data for IMAX
    // ---------------------------------------------------------
    printf("sizeof(int)=%d\n", sizeof(int));
    printf("sizeof(float)=%d\n", sizeof(float));
    FILE *file;
    int *array_load_temp;
    int array_length = IC*D*H*W;  // You should know the length of the array beforehand

    // Open the .bin file in read mode
    file = fopen(IN_PATH, "rb");
    array_load_temp = (int*) malloc(array_length * sizeof(float));
    fread(array_load_temp, sizeof(float), array_length, file);
    fclose(file);

    //load data to in
  for (ic=0; ic<IC; ic++) {
    for (dep=0; dep<D; dep++) {
      for (row=0; row<H; row++) {
	      for (col=0; col<W; col++) {
	  *(float*)&in[ic*D*H*W+dep*H*W+row*W+col] = *(float*)&array_load_temp[ic*D*H*W+dep*H*W+row*W+col];
  }}}}

  // Use the array (Printing in this example)
    printf("input is: \n");
    for (int i = 0; i < 10; ++i) {
        printf("%f ", *(float*)&in[i]);
    }
    printf("\n");

    // Free the allocated memory
    free(array_load_temp);



    // ---------------------------------------------------------
    // load kernel for IMAX
    // ---------------------------------------------------------
    array_length = OC*IC*K*K*K;  // You should know the length of the array beforehand

    // Open the .bin file in read mode
    file = fopen(KER_PATH, "rb");
    if (file == NULL) {
        printf("Unable to open the file.\n");
        return 1;
    }

    array_load_temp = (int*) malloc(array_length * sizeof(float));
    if (array_load_temp == NULL) {
        printf("Memory allocation failed.\n");
        return 1;
    }
    fread(array_load_temp, sizeof(float), array_length, file);
    fclose(file);

    //load data to in
  for (oc=0; oc<OC; oc++) {
    for (ic=0; ic<IC; ic++) {
      for (z=0; z<K; z++) {
        for (y=0; y<K; y++) {
          for (x=0; x<K; x++) {
            *(float*)&ker[ic*OC*K*K*K+oc*K*K*K+z*K*K+y*K+x] = *(float*)&array_load_temp[ic*OC*K*K*K+oc*K*K*K+z*K*K+y*K+x];
          }}}}}

    
    // Use the array (Printing in this example)
    printf("ker is: \n");
    for (int i = 0; i < 10; ++i) {
        printf("%f ", *(float*)&ker[i]);
    }
    printf("\n");

    // Free the allocated memory
    free(array_load_temp);



#if !defined(ARMSIML)
  x11_open(0);
#endif

#if defined(EMAX6)
  reset_nanosec();
  // orig();
  get_nanosec(0);
  show_nanosec();
#endif
#if defined(EMAX7)
  reset_nanosec(0);
  // orig();
  get_nanosec(0,0);
  show_nanosec(0);
#endif

#if defined(EMAX6)
  reset_nanosec();
  imax(0);
  get_nanosec(0);
  show_nanosec();
#endif
#if defined(EMAX7)
  reset_nanosec(0);
  imax(0);
  get_nanosec(0,0);
  show_nanosec(0);
#endif


#ifdef ARMSIML
  copy_Z(0, out1); _copyX(0, Z);
  copy_Z(1, out1); _copyX(1, Z);
  copy_Z(2, out1); _copyX(2, Z);
  copy_Z(3, out1); _copyX(3, Z);
  copy_Z(5, out1); _copyX(4, Z);
  copy_Z(6, out1); _copyX(5, Z);
  copy_Z(7, out1); _copyX(6, Z);
  copy_Z(8, out1); _copyX(7, Z);
  copy_Z(10,out1); _copyX(8 ,Z);
  copy_Z(11,out1); _copyX(9 ,Z);
  copy_Z(12,out1); _copyX(10,Z);
  copy_Z(13,out1); _copyX(11,Z);
  _updateX();
#endif
#if !defined(ARMSIML)
  copy_Z(0, out1); BGR_to_X(0, Z);
  copy_Z(1, out1); BGR_to_X(1, Z);
  copy_Z(2, out1); BGR_to_X(2, Z);
  copy_Z(3, out1); BGR_to_X(3, Z);
  copy_Z(4, out1); BGR_to_X(4, Z);
  copy_Z(5, out1); BGR_to_X(5, Z);
  copy_Z(6, out1); BGR_to_X(6, Z);
  copy_Z(7, out1); BGR_to_X(7, Z);
  copy_Z(8, out1); BGR_to_X(8, Z);
  copy_Z(9 ,out1); BGR_to_X(9, Z);
  copy_Z(10,out1); BGR_to_X(10,Z);
  copy_Z(11,out1); BGR_to_X(11,Z);
  copy_Z(12,out1); BGR_to_X(12,Z);
  copy_Z(13,out1); BGR_to_X(13,Z);
  copy_Z(14,out1); BGR_to_X(14,Z);
  copy_Z(15,out1); BGR_to_X(15,Z);
  x11_update();
#endif

printf("Num of MULT: orig=%d imax=%d\n", count0, count1);

FILE *fileout = fopen(OUT_PATH, "wb");  // Open a binary file for writing
if (fileout != NULL) {
    printf("OC*D*H*W=%d\n", OC*D*H*W);
    printf("sizeof(float)=%d\n", sizeof(float));
    fwrite(out1, sizeof(float), OC*D*H*W, fileout);  // Write the data
    fclose(fileout);  // Close the file
    printf("Wrote file out\n");
} else {
    printf("Failed to open file for out writing.\n");
}

// count2 = 0;
// double tolerance, diff, abs_out0, abs_out1;
// for (oc=0; oc<OC; oc++) {
//     for (dep=1; dep<D-1; dep++) {
//         for (row=1; row<H-1; row++) {
//             for (col=0; col<W-2; col++) {

//                 double value_out0 = (double)*(float*)&out0[oc*D*H*W+dep*H*W+row*W+col];
//                 double value_out1 = (double)*(float*)&out1[oc*D*H*W+dep*H*W+row*W+col];

//                 diff = value_out0 - value_out1;
//                 abs_out0 = (value_out0 > 0) ? value_out0 : -value_out0;
//                 abs_out1 = (value_out1 > 0) ? value_out1 : -value_out1;

//                 // 許容誤差を3%とする
//                 tolerance = 0.03 * ((abs_out0 > abs_out1) ? abs_out0 : abs_out1);
//                 double error_rate = (diff / ((abs_out0 > abs_out1) ? abs_out0 : abs_out1)) * 100;

//                 if (diff < -tolerance || diff > tolerance) {
//                     count2++;
//                     printf("o0[%d]=%f o1[%d]=%f diff=%f tolerance=%f error_rate=%.2f%%\n",
//                            oc*D*H*W+dep*H*W+row*W+col, value_out0,
//                            oc*D*H*W+dep*H*W+row*W+col, value_out1,
//                            diff, tolerance, error_rate);
//                 }
//             }
//         }
//     }
// }
// if (count2)
//     printf("Num of diffs: %d\n", count2);
// else
//     printf("Results are equal\n");



#if defined(EMAX6)
  show_nanosec();
#endif
#if defined(EMAX7)
  show_nanosec(0);
#endif
  return 0;
#if !defined(ARMSIML)
  printf("==== Normal end. Type any in ImageWin ====\n");
  while (!x11_checkevent());
#endif
}

copy_Z(id, from)
     int id; /* 0 .. 11 */
     unsigned int *from;
{
  int i, j;
  volatile unsigned int *to = Z;

  switch (id) {
  case 0:                   break;
  case 1:  from += H*W;     break;
  case 2:  from += H*W*2;   break;
  case 3:  from += H*W*3;   break;
  case 4:  from += H*W*4;   break;
  case 5:  from += H*W*5;   break;
  case 6:  from += H*W*6;   break;
  case 7:  from += H*W*7;   break;
  case 8:  from += H*W*8;   break;
  case 9:  from += H*W*9;   break;
  case 10: from += H*W*10;  break;
  case 11: from += H*W*11;  break;
  case 12: from += H*W*12;  break;
  case 13: from += H*W*13;  break;
  case 14: from += H*W*14;  break;
  case 15: from += H*W*15;  break;
  }
  for (i=0; i<HT; i++, from+=W) {
    if (i<H) {
      for (j=0; j<WD; j++) {
        if (j<W) *to++ = (*(from+j))<<2;
        else     *to++ = 0;
      }
    }
    else {
      for (j=0; j<WD; j++)
        *to++ = 0;
    }
  }
}

orig() {
  printf("<<<ORIG>>>\n");
  for (ic=0; ic<IC; ic++) { /* set input channel */
    ip0 = &in[ic*D*H*W]; /* top of input */
    for (dep=1; dep<D-1; dep++) { /* image loop */
      for (row=1; row<H-1; row++) { /* image loop */
	for (col=0; col<W-2; col++) {
	  for (oc=0; oc<OC; oc++) { /* set output channel */
	    op = &out0[oc*D*H*W+dep*H*W+row*W+col]; /* top of output */
	    kp = &ker[(oc*IC+ic)*K*K*K];
	    kidx = 0;
	    for (z=-((K-1)/2); z<=(K-1)/2; z++) { /* kernel loop */
	      for (y=-((K-1)/2); y<=(K-1)/2; y++) { /* kernel loop */
		for (x=-((K-1)/2); x<=(K-1)/2; x++) {
		  if (ic == 0 && kidx == 0) {
		    *(float*)&*op  = *(float*)&ip0[(dep+z)*H*W+(row+y)*W+col+x+1] * *(float*)&kp[kidx];
		    /*printf("head [%d %d %d][%d %d %d] out0[%d]=%d\n", ic, row, col, oc, y, x, op-&out0[0], *op);*/
		  }
		  else {
		    *(float*)&*op += *(float*)&ip0[(dep+z)*H*W+(row+y)*W+col+x+1] * *(float*)&kp[kidx];
		    /*printf("     [%d %d %d][%d %d %d] out0[%d]=%d\n", ic, row, col, oc, y, x, op-&out0[0], *op);*/
		  }
		  kidx++;
		  count0++;
		}
	      }
	    }
          }
        }
      }
    }
  }
}

#if 0
/*     +-----------------+                       */
/*     | z-----------------+                     */
/*     | | +--x--------------+                   */
/*     | | | +-------242-------+                 */
/*     | | y |       240       | 968B*18=17424B  */
/*     | | | |                 | 968B            */
/*     | | | |     * * *       | 968B            */
/*     | | | |     * * *       | 968B            */
/*     +-| | |     * * *       | 968B            */
/*       +-| |                 | 968B            */
/*         +-|                 | 968B            */
/*           +-----------------+                 */
imax(int LANE) {
  Ull CHIP;
  Ull rofs;
  printf("<<<IMAX>>>\n");
  for (ztop=1; ztop<M-1; ztop++) { /* Z */
    for (top=1; top<M-1; top+=RMGRP) { /* Y */
      for (iset=0; iset<IC; iset+=IMAP) { /* accumulate multiple sets of IC */
	for (oc=0; oc<OC/NCHIP; oc+=W) { /* set output channel */
    /*3*/ for (CHIP=0; CHIP<NCHIP; CHIP++) { /* output channels are parallelized by multi-chip (OC/#chip) */
      /*2*/ for (rofs=0; rofs<RMGRP; rofs++) { /* image loop (row) */
        /*1*/ for (col=0; col<M-2; col++) { /* image loop (col) */
                for (w=0; w<W; w++) { /* set output channel */
		  op = &out1[(CHIP*OC/NCHIP+oc+w)*M*M*M+ztop*M*M+(top+rofs)*M+col]; /* top of output */
		  for (ic=0; ic<IMAP; ic++) { /* set offset of input channel */
		    ip0 = &in[(iset+ic)*M*M*M]; /* top of input */
		    kp = &ker[((CHIP*OC/NCHIP+oc+w)*IC+iset+ic)*K*K*K];
		    kidx = 0;
		    for (z=-((K-1)/2); z<=(K-1)/2; z++) { /* kernel loop */
		      for (y=-((K-1)/2); y<=(K-1)/2; y++) {
			for (x=-((K-1)/2); x<=(K-1)/2; x++) {
			  if (iset == 0 && ic == 0 && kidx == 0) {
			    *(float*)&*op  = *(float*)&ip0[(ztop+z)*M*M+(top+rofs+y)*M+col+x+1] * *(float*)&kp[kidx];
			    /*printf("head [%d %d %d][%d %d %d] out1[%d]=%d\n", ic, row, col, (oc+w), y, x, op-&out1[0], *op);*/
			  }
			  else {
			    *(float*)&*op += *(float*)&ip0[(ztop+z)*M*M+(top+rofs+y)*M+col+x+1] * *(float*)&kp[kidx];
			    /*printf("     [%d %d %d][%d %d %d] out1[%d]=%d\n", ic, row, col, (oc+w), y, x, op-&out1[0], *op);*/
			  }
			  kidx++;
			  count1++;
			}
		      }
                    }
                  }
                }
              }
            }
          }
        }
      }
    }
  }
}

#else

imax(int LANE) {
  int count_drain =0, count_end = 0;
  Ull  CHIP;
  Ull  LOOP1, LOOP0;
  Ull  INIT1, INIT0;
  Ull  AR[64][4];                     /* output of EX     in each unit */
  Ull  BR[64][4][4];                  /* output registers in each unit */
  Ull  r0, r1, r2, r3, r4, r5, r6, r7, r8, r9, r10, r11, r12, r13, r14, r15;
  Ull  r16, r17, r18, r19, r20, r21, r22, r23, r24, r25, r26, r27, r28, r29, r30, r31;
  Ull  cc0, cc1, cc2, cc3, ex0, ex1;
  Ull  cofs, rofs, oofs, k;

  printf("<<<IMAX>>>\n");
  for (ztop=1; ztop<D-1; ztop++) { /* depth */
    for (top=1; top<H-1; top+=RMGRP) {
      for (iset=0; iset<IC; iset+=IMAP) { /* accumulate multiple sets of IC */
	Uint *ip[IMAP], *it[3][IMAP], *ip0[IMAP][K*K*K], *ip1[IMAP][K*K*K];
	for (k=0; k<IMAP; k++) {
    // inside one chnnel
	  ip[k] = &in[(iset+k)*D*H*W]; /* top of input#0-5 */
	  it[0][k] = ip[k]+(ztop-1)*H*W+(top-1)*W+1-1; // 
	  it[1][k] = ip[k]+(ztop+0)*H*W+(top-1)*W+1-1;
	  it[2][k] = ip[k]+(ztop+1)*H*W+(top-1)*W+1-1;
	  ip0[k][ 0] = ip[k]+(ztop-1)*H*W+(top-1)*W+1-1; ip0[k][ 1] = ip[k]+(ztop-1)*H*W+(top-1)*W+1+0; ip0[k][ 2] = ip[k]+(ztop-1)*H*W+(top-1)*W+1+1;
	  ip0[k][ 3] = ip[k]+(ztop-1)*H*W+(top+0)*W+1-1; ip0[k][ 4] = ip[k]+(ztop-1)*H*W+(top+0)*W+1+0; ip0[k][ 5] = ip[k]+(ztop-1)*H*W+(top+0)*W+1+1;
	  ip0[k][ 6] = ip[k]+(ztop-1)*H*W+(top+1)*W+1-1; ip0[k][ 7] = ip[k]+(ztop-1)*H*W+(top+1)*W+1+0; ip0[k][ 8] = ip[k]+(ztop-1)*H*W+(top+1)*W+1+1;
	  ip0[k][ 9] = ip[k]+(ztop+0)*H*W+(top-1)*W+1-1; ip0[k][10] = ip[k]+(ztop+0)*H*W+(top-1)*W+1+0; ip0[k][11] = ip[k]+(ztop+0)*H*W+(top-1)*W+1+1;
	  ip0[k][12] = ip[k]+(ztop+0)*H*W+(top+0)*W+1-1; ip0[k][13] = ip[k]+(ztop+0)*H*W+(top+0)*W+1+0; ip0[k][14] = ip[k]+(ztop+0)*H*W+(top+0)*W+1+1;
	  ip0[k][15] = ip[k]+(ztop+0)*H*W+(top+1)*W+1-1; ip0[k][16] = ip[k]+(ztop+0)*H*W+(top+1)*W+1+0; ip0[k][17] = ip[k]+(ztop+0)*H*W+(top+1)*W+1+1;
	  ip0[k][18] = ip[k]+(ztop+1)*H*W+(top-1)*W+1-1; ip0[k][19] = ip[k]+(ztop+1)*H*W+(top-1)*W+1+0; ip0[k][20] = ip[k]+(ztop+1)*H*W+(top-1)*W+1+1;
	  ip0[k][21] = ip[k]+(ztop+1)*H*W+(top+0)*W+1-1; ip0[k][22] = ip[k]+(ztop+1)*H*W+(top+0)*W+1+0; ip0[k][23] = ip[k]+(ztop+1)*H*W+(top+0)*W+1+1;
	  ip0[k][24] = ip[k]+(ztop+1)*H*W+(top+1)*W+1-1; ip0[k][25] = ip[k]+(ztop+1)*H*W+(top+1)*W+1+0; ip0[k][26] = ip[k]+(ztop+1)*H*W+(top+1)*W+1+1;
	  ip1[k][ 0] = ip[k]+(ztop-1)*H*W+(top-1)*W+1+1; ip1[k][ 1] = ip[k]+(ztop-1)*H*W+(top-1)*W+1+2; ip1[k][ 2] = ip[k]+(ztop-1)*H*W+(top-1)*W+1+3;
	  ip1[k][ 3] = ip[k]+(ztop-1)*H*W+(top+0)*W+1+1; ip1[k][ 4] = ip[k]+(ztop-1)*H*W+(top+0)*W+1+2; ip1[k][ 5] = ip[k]+(ztop-1)*H*W+(top+0)*W+1+3;
	  ip1[k][ 6] = ip[k]+(ztop-1)*H*W+(top+1)*W+1+1; ip1[k][ 7] = ip[k]+(ztop-1)*H*W+(top+1)*W+1+2; ip1[k][ 8] = ip[k]+(ztop-1)*H*W+(top+1)*W+1+3;
	  ip1[k][ 9] = ip[k]+(ztop+0)*H*W+(top-1)*W+1+1; ip1[k][10] = ip[k]+(ztop+0)*H*W+(top-1)*W+1+2; ip1[k][11] = ip[k]+(ztop+0)*H*W+(top-1)*W+1+3;
	  ip1[k][12] = ip[k]+(ztop+0)*H*W+(top+0)*W+1+1; ip1[k][13] = ip[k]+(ztop+0)*H*W+(top+0)*W+1+2; ip1[k][14] = ip[k]+(ztop+0)*H*W+(top+0)*W+1+3;
	  ip1[k][15] = ip[k]+(ztop+0)*H*W+(top+1)*W+1+1; ip1[k][16] = ip[k]+(ztop+0)*H*W+(top+1)*W+1+2; ip1[k][17] = ip[k]+(ztop+0)*H*W+(top+1)*W+1+3;
	  ip1[k][18] = ip[k]+(ztop+1)*H*W+(top-1)*W+1+1; ip1[k][19] = ip[k]+(ztop+1)*H*W+(top-1)*W+1+2; ip1[k][20] = ip[k]+(ztop+1)*H*W+(top-1)*W+1+3;
	  ip1[k][21] = ip[k]+(ztop+1)*H*W+(top+0)*W+1+1; ip1[k][22] = ip[k]+(ztop+1)*H*W+(top+0)*W+1+2; ip1[k][23] = ip[k]+(ztop+1)*H*W+(top+0)*W+1+3;
	  ip1[k][24] = ip[k]+(ztop+1)*H*W+(top+1)*W+1+1; ip1[k][25] = ip[k]+(ztop+1)*H*W+(top+1)*W+1+2; ip1[k][26] = ip[k]+(ztop+1)*H*W+(top+1)*W+1+3;
	}

	for (oc=0; oc<OC/NCHIP; oc+=W_) { /* set output channel */
	  Uint *kp0[IMAP][NCHIP], *kp1[IMAP][NCHIP], *kp2[IMAP][NCHIP], *kp3[IMAP][NCHIP];
	  Uint *op0[NCHIP], *op1[NCHIP], *op2[NCHIP], *op3[NCHIP];
	  Uint *ot0[NCHIP], *ot1[NCHIP], *ot2[NCHIP], *ot3[NCHIP];

	  for (CHIP=0; CHIP<NCHIP; CHIP++) { /* output channels are parallelized by multi-chip (OC/#chip) */
	    Uint choc  = CHIP*OC/NCHIP+oc;
	    for (k=0; k<IMAP; k++) {
	      kp0[k][CHIP] = ker+((choc+0)*IC+iset+k)*K*K*K;
	      kp1[k][CHIP] = ker+((choc+1)*IC+iset+k)*K*K*K;
	      kp2[k][CHIP] = ker+((choc+2)*IC+iset+k)*K*K*K;
	      kp3[k][CHIP] = ker+((choc+3)*IC+iset+k)*K*K*K;
	    }
	    op0[CHIP] = out1+(choc+0)*D*H*W+ztop*H*W+top*W+0; op1[CHIP] = out1+(choc+1)*D*H*W+ztop*H*W+top*W+0; op2[CHIP] = out1+(choc+2)*D*H*W+ztop*H*W+top*W+0; op3[CHIP] = out1+(choc+3)*D*H*W+ztop*H*W+top*W+0;
	    ot0[CHIP] = out1+(choc+0)*D*H*W+ztop*H*W+top*W+0; ot1[CHIP] = out1+(choc+1)*D*H*W+ztop*H*W+top*W+0; ot2[CHIP] = out1+(choc+2)*D*H*W+ztop*H*W+top*W+0; ot3[CHIP] = out1+(choc+3)*D*H*W+ztop*H*W+top*W+0;
	  }

#define cnn_core1(r, i, j, ofs, k, rp1) \
            mop(OP_LDWR,   1, &BR[r][0][1],  (Ull)kp0[i][CHIP], ofs, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K);\
            mop(OP_LDWR,   1, &BR[r][0][0],  (Ull)kp1[i][CHIP], ofs, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K);\
            mop(OP_LDWR,   1, &BR[r][1][1],  (Ull)kp2[i][CHIP], ofs, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K);\
            mop(OP_LDWR,   1, &BR[r][1][0],  (Ull)kp3[i][CHIP], ofs, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K);\
            mop(OP_LDR,    1, &BR[r][2][1],  (Ull)ip1[i][k], oofs, MSK_W0, (Ull)it[j][i], W*(RMGRP+2), 0, 0, (Ull)NULL, W*(RMGRP+2));\
            mop(OP_LDR,    1, &BR[r][2][0],  (Ull)ip0[i][k], oofs, MSK_W0, (Ull)it[j][i], W*(RMGRP+2), 0, 0, (Ull)NULL, W*(RMGRP+2));\
            exe(OP_FMA, &AR[rp1][0], AR[r][0], EXP_H3210, BR[r][2][0], EXP_H3210, BR[r][0][1], EXP_H1010, OP_NOP, 0LL, OP_NOP, 0LL);\
            exe(OP_FMA, &AR[rp1][1], AR[r][1], EXP_H3210, BR[r][2][0], EXP_H3210, BR[r][0][0], EXP_H1010, OP_NOP, 0LL, OP_NOP, 0LL);\
            exe(OP_FMA, &AR[rp1][2], AR[r][2], EXP_H3210, BR[r][2][0], EXP_H3210, BR[r][1][1], EXP_H1010, OP_NOP, 0LL, OP_NOP, 0LL);\
            exe(OP_FMA, &AR[rp1][3], AR[r][3], EXP_H3210, BR[r][2][0], EXP_H3210, BR[r][1][0], EXP_H1010, OP_NOP, 0LL, OP_NOP, 0LL)

#define cnn_final(r, rp1) \
            mop(OP_LDR,  1, &BR[rp1][0][1],  (Ull)op0[CHIP], oofs, MSK_W0, (Ull)ot0[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP);\
            mop(OP_LDR,  1, &BR[rp1][1][1],  (Ull)op1[CHIP], oofs, MSK_W0, (Ull)ot1[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP);\
            mop(OP_LDR,  1, &BR[rp1][2][1],  (Ull)op2[CHIP], oofs, MSK_W0, (Ull)ot2[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP);\
            mop(OP_LDR,  1, &BR[rp1][3][1],  (Ull)op3[CHIP], oofs, MSK_W0, (Ull)ot3[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP);\
            exe(OP_FAD, &AR[rp1][0], AR[r][0], EXP_H3210, BR[rp1][0][1], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);\
            exe(OP_FAD, &AR[rp1][1], AR[r][1], EXP_H3210, BR[rp1][1][1], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);\
            exe(OP_FAD, &AR[rp1][2], AR[r][2], EXP_H3210, BR[rp1][2][1], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);\
            exe(OP_FAD, &AR[rp1][3], AR[r][3], EXP_H3210, BR[rp1][3][1], EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);\
            mop(OP_STR,  3, &AR[rp1][0], oofs, (Ull)op0[CHIP], MSK_D0, (Ull)ot0[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP);\
            mop(OP_STR,  3, &AR[rp1][1], oofs, (Ull)op1[CHIP], MSK_D0, (Ull)ot1[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP);\
            mop(OP_STR,  3, &AR[rp1][2], oofs, (Ull)op2[CHIP], MSK_D0, (Ull)ot2[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP);\
            mop(OP_STR,  3, &AR[rp1][3], oofs, (Ull)op3[CHIP], MSK_D0, (Ull)ot3[CHIP], W*RMGRP, 0, 1, (Ull)NULL, W*RMGRP)
//EMAX5A begin cnn mapdist=0
    /*3*/ for (CHIP=0; CHIP<NCHIP; CHIP++) { /* output channels are parallelized by multi-chip (OC/#chip) */
      /*2*/ for (INIT1=1,LOOP1=RMGRP,rofs=0-H*4; LOOP1--; INIT1=0) {            /* stage#0 *//* mapped to FOR() on BR[63][1][0] */
        /*1*/ for (INIT0=1,LOOP0=(W-2)/2,cofs=0-4; LOOP0--; INIT0=0) {          /* stage#0 *//* mapped to FOR() on BR[63][0][0] */
    exe(OP_ADD,    &cofs, INIT0?cofs:cofs, EXP_H3210, 8, EXP_H3210, 0LL, EXP_H3210, OP_AND, 0x00000000ffffffffLL, OP_NOP, 0LL);/* stage#0: cof+=8 */
		exe(OP_ADD,    &rofs, rofs, EXP_H3210, INIT0?H*4:0,  EXP_H3210, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL);           /* stage#0: rof+=H*4(init) or 0(not init) */
		exe(OP_ADD,    &oofs, rofs, EXP_H3210, cofs, EXP_H3210, 0LL, EXP_H3210, OP_AND, 0x00000000ffffffffLL, OP_NOP, 0LL);  /* stage#1: oof=rof+cof */
    // printf("rofs=%d, cofs=%d, oofs=%d\n", rofs, cofs, oofs);
    // printf("kp0[0][CHIP]=%p, kp1[0][CHIP]=%p, kp2[0][CHIP]=%p, kp3[0][CHIP]=%p\n", kp0[0][CHIP], kp1[0][CHIP], kp2[0][CHIP], kp3[0][CHIP]);
    // printf("ip1[0][0]=%p, ip0[0][0]=%p\n", ip1[0][0], ip0[0][0]);
    // printf("op0[CHIP]=%p, op1[CHIP]=%p, op2[CHIP]=%p, op3[CHIP]=%p\n", op0[CHIP], op1[CHIP], op2[CHIP], op3[CHIP]);

		mop(OP_LDWR,   1, &BR[2][0][1],  (Ull)kp0[0][CHIP], 0LL, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K); /* stage#2 */
		mop(OP_LDWR,   1, &BR[2][0][0],  (Ull)kp1[0][CHIP], 0LL, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K); /* stage#2 */
		mop(OP_LDWR,   1, &BR[2][1][1],  (Ull)kp2[0][CHIP], 0LL, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K); /* stage#2 */
		mop(OP_LDWR,   1, &BR[2][1][0],  (Ull)kp3[0][CHIP], 0LL, MSK_D0, (Ull)ker, IC*OC*K*K*K, 0, 0, (Ull)NULL, IC*OC*K*K*K); /* stage#2 10KB */
		mop(OP_LDR,    1, &BR[2][2][1],  (Ull)ip1[0][0], oofs, MSK_W0, (Ull)it[0][0], W*(RMGRP+2), 0, 0, (Ull)NULL, W*(RMGRP+2)); /* stage#2 8KB *//* unaligned load */
		mop(OP_LDR,    1, &BR[2][2][0],  (Ull)ip0[0][0], oofs, MSK_W0, (Ull)it[0][0], W*(RMGRP+2), 0, 0, (Ull)NULL, W*(RMGRP+2)); /* stage#2 8KB *//* unaligned load */
		exe(OP_FML, &AR[3][0], BR[2][2][0], EXP_H3210, BR[2][0][1], EXP_H1010, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL); /* stage#3 */
		exe(OP_FML, &AR[3][1], BR[2][2][0], EXP_H3210, BR[2][0][0], EXP_H1010, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL); /* stage#3 */
		exe(OP_FML, &AR[3][2], BR[2][2][0], EXP_H3210, BR[2][1][1], EXP_H1010, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL); /* stage#3 */
		exe(OP_FML, &AR[3][3], BR[2][2][0], EXP_H3210, BR[2][1][0], EXP_H1010, 0LL, EXP_H3210, OP_NOP, 0LL, OP_NOP, 0LL); /* stage#3 */

		cnn_core1( 3, 0, 0,  4LL, 1,  4); // r,i,j,ofs,k,rp1
		cnn_core1( 4, 0, 0,  8LL, 2,  5);
		cnn_core1( 5, 0, 0, 12LL, 3,  6);
		cnn_core1( 6, 0, 0, 16LL, 4,  7);
		cnn_core1( 7, 0, 0, 20LL, 5,  8);
		cnn_core1( 8, 0, 0, 24LL, 6,  9);
		cnn_core1( 9, 0, 0, 28LL, 7, 10);
		cnn_core1(10, 0, 0, 32LL, 8, 11);

		cnn_core1(11, 0, 1, 36LL, 9, 12);
		cnn_core1(12, 0, 1, 40LL,10, 13);
		cnn_core1(13, 0, 1, 44LL,11, 14);
		cnn_core1(14, 0, 1, 48LL,12, 15);
		cnn_core1(15, 0, 1, 52LL,13, 16);
		cnn_core1(16, 0, 1, 56LL,14, 17);
		cnn_core1(17, 0, 1, 60LL,15, 18);
		cnn_core1(18, 0, 1, 64LL,16, 19);
		cnn_core1(19, 0, 1, 68LL,17, 20);

		cnn_core1(20, 0, 2, 72LL,18, 21);
		cnn_core1(21, 0, 2, 76LL,19, 22);
		cnn_core1(22, 0, 2, 80LL,20, 23);
		cnn_core1(23, 0, 2, 84LL,21, 24);
		cnn_core1(24, 0, 2, 88LL,22, 25);
		cnn_core1(25, 0, 2, 92LL,23, 26);
		cnn_core1(26, 0, 2, 96LL,24, 27);
		cnn_core1(27, 0, 2,100LL,25, 28);
		cnn_core1(28, 0, 2,104LL,26, 29);
#if (IMAP==1)
		cnn_final(29,     30);
#endif
#if (IMAP>1)
		cnn_core1(29, 1, 0,  0LL, 0, 30);
		cnn_core1(30, 1, 0,  4LL, 1, 31);
		cnn_core1(31, 1, 0,  8LL, 2, 32);
		cnn_core1(32, 1, 0, 12LL, 3, 33);
		cnn_core1(33, 1, 0, 16LL, 4, 34);
		cnn_core1(34, 1, 0, 20LL, 5, 35);
		cnn_core1(35, 1, 0, 24LL, 6, 36);
		cnn_core1(36, 1, 0, 28LL, 7, 37);
		cnn_core1(37, 1, 0, 32LL, 8, 38);

		cnn_core1(38, 1, 1, 36LL, 9, 39);
		cnn_core1(39, 1, 1, 40LL,10, 40);
		cnn_core1(40, 1, 1, 44LL,11, 41);
		cnn_core1(41, 1, 1, 48LL,12, 42);
		cnn_core1(42, 1, 1, 52LL,13, 43);
		cnn_core1(43, 1, 1, 56LL,14, 44);
		cnn_core1(44, 1, 1, 60LL,15, 45);
		cnn_core1(45, 1, 1, 64LL,16, 46);
		cnn_core1(46, 1, 1, 68LL,17, 47);

		cnn_core1(47, 1, 2, 72LL,18, 48);
		cnn_core1(48, 1, 2, 76LL,19, 49);
		cnn_core1(49, 1, 2, 80LL,20, 50);
		cnn_core1(50, 1, 2, 84LL,21, 51);
		cnn_core1(51, 1, 2, 88LL,22, 52);
		cnn_core1(52, 1, 2, 92LL,23, 53);
		cnn_core1(53, 1, 2, 96LL,24, 54);
		cnn_core1(54, 1, 2,100LL,25, 55);
		cnn_core1(55, 1, 2,104LL,26, 56);
#endif
#if (IMAP==2)
		cnn_final(56,     57);
#endif
              }
            }
          }
//EMAX5A end
count_end++;
        }
      }
    }
  }
//EMAX5A drain_dirty_lmm
count_drain++;
printf("count_drain=%d, count_end=%d\n", count_drain, count_end);
}
#endif
