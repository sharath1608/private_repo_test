/*
 * cp_segy: SEGY format data copy utility.  See "USAGE" below for arguments and meanings
 * Received from Paul Shields
 * Revised by Bill Menger 12/3/2013 - repair byte-swap on header 2 in trace headers
 *                                  - reset trace counter for multiple file option
*/
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h> 
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mtio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>

#ifndef SEEK_SET
#define SEEK_SET 0
#endif

#define DEBUG 0

#define bcopy(i, o, n) memcpy(o, i, n)

#define IEMAXIBM 0x611fffff
#define IEMINIBM 0x21200000
#define IEEEMAX  0x7fffffff

static char rcsid[] = "$Id: cp_segy.c,v 1.5 2006/05/25 21:11:02 release Exp $";

void ibm2ieee(in, out, nb)
int *in, *out, nb;
{
    int i;
    static int mt[] = { 8, 4, 2, 2, 1, 1, 1, 1};
    static int it[] = { 0x21800000, 0x21400000, 0x21000000, 0x21000000,
			    0x20c00000, 0x20c00000, 0x20c00000, 0x20c00000};

    for( i = 0 ; i < nb ; i++ ) {
	register int inabs = in[i]&0x7fffffff;
	if( inabs > IEMAXIBM ) 
	    out[i] = IEEEMAX | (in[i]&0x80000000);
	else if( inabs < IEMINIBM )
	    out[i] = 0;
	else {
	    register int mant = in[i]&0xffffff;
	    register int ix = mant>>21;
	    register iexp = (in[i]&0x7f000000) - it[ix];
	    mant = mant*mt[ix]+iexp*2;
	    out[i] = mant | (in[i]&0x80000000);
	}
    }
}

void ieee2ibm(in, out, nb)
int *in, *out, nb;
{
    int i;
    static int mt[] = {2, 1, 0, 3}; 
    /* Shift mantissa to have a multiple of 16 */

    static int it[] = { 0x21200000, 0x21400000, 0x21800000, 0x22100000};
    /* Bias to add to the exponent and high part of mantissa */

    for( i = 0 ; i < nb ; i++ ) {
	if( in[i] == 0 )
	    out[i] = 0;
	else {
	    register int ix = (in[i]>>23) & 0x3;
	    register int mant = (in[i] & 0x7fffff)>>mt[ix];
	    register int iexp = ((in[i]&0x7e000000)>>1) + it[ix];
	    out[i] = (mant+iexp) | (in[i]&0x80000000);
	}
    }
}


/*
                HEADER OF SEGY
                from :
        'Recommended standards for digital tape formats '
                        BARRY & al
                SEG - 1975

*/

typedef int BYTE4;
typedef short BYTE2;


typedef struct segy_hd {
        BYTE4   job_number;     /*  job identification number  */
        BYTE4   line_number;    /*  line number */
        BYTE4   reel_number;    /*  reel number */
        BYTE2   nb_tra_rec;     /*  number of traces per record */
        BYTE2   nb_aux_rec;     /*  number of auxiliary traces per record */
        BYTE2   sampling;       /*  sampling interval ( for the reel ) */
        BYTE2   sampl_fld;      /*  sampling interval ( for original rec. ) */
        BYTE2   nb_samples;     /*  number of samples ( for the reel )  */
        BYTE2   nb_samp_fld;    /*  number of samples ( for original rec.) */
        BYTE2   data_form;      /*  data format code 1 : floating point
                                2 : fixed point ( 4 byte ) 3 : fixed point 
                                4 : fixed point with gain  */
        BYTE2   cdp_fold;       /*  CDP fold */
        BYTE2   trace_sort;     /*  trace sorting  */
        BYTE2   vert_sum;       /*  vertical sum */
        BYTE2   swp_start;      /*   sweep frequency at start  */
        BYTE2   swp_end;        /*   sweep frequency at end  */
        BYTE2   swp_length;     /*   sweep length     */
        BYTE2   swp_type;       /*   sweep type 1 : linear 2 : parabolic
                                                3 : exponential 4 :other */
        BYTE2   swp_channel;    /*   trace number of sweep channel  */
        BYTE2   swp_tap_st;     /*   sweep trace taper length at start */
        BYTE2   swp_tap_ed;     /*   sweep trace taper length at end */
        BYTE2   taper_type;     /*   taper type 1 : linear 2 : cos 3 : other*/
        BYTE2   correlated;     /*   correlated 1 : no 2 : yes */
        BYTE2   bin_gain;       /*   binary gain recovered  1 : no 2 : yes */
        BYTE2   amp_recover;    /*   amplitude recovery method  */
        BYTE2   meas_sys;       /*   measurement system 1 : meters 2 : feets */
        BYTE2   polarity;       /*   impulse signal polarity  */
        BYTE2   vib_pol;        /*   vibratory polarity */
        char    unass[340];     /*   unassigned  */
}  SEGY_HD;
 
#define  LG_SEGY_HD sizeof(SEGY_HD)


typedef struct  segy_tr_hd {
        BYTE4   traseqlin;      /*   trace sequence nb in line  */
        BYTE4   traseqrel;      /*   trace sequence nb in reel  */
        BYTE4   field_rec;      /*   original field record number */
        BYTE4   tracnb_fld;     /*   trace nb in field record  */
        BYTE4   esp;            /*   energy source point number */
        BYTE4   cdp_ens;        /*   CDP ensemble number */
        BYTE4   tr_in_cdp;      /*   trace number in CDP  */
        BYTE2   trace_id;       /*   trace identification  */
        BYTE2   nbvst;  /*   nb of vertically summed traces */
        BYTE2   nbhst;  /*   nb of horizontally stacked traces */
        BYTE2   data_use;       /*   data use 1 : production 2: test  */
        BYTE4   srdist; /*   distance from source to receiver */
        BYTE4   rcv_elev;       /*   receiver elevation  */
        BYTE4   src_elec;       /*   source elevation  */
        BYTE4   src_depth;      /*   source depth below surface  (> 0)  */
        BYTE4   drcv_elev;      /*   datum elevation at receiver */
        BYTE4   dsrc_elev;      /*   datum elevation at source   */
        BYTE4   wsrc_depth;     /*   water depth at source  */
        BYTE4   wgrp_depth;     /*   water depth at group   */
        BYTE2   scaler_dep;     /*   scaler use on all elevations and depth */
        BYTE2   scaler_cor;     /*   scaler use on all coordinates   */
        BYTE4   src_X;  /*   source coordinate X */
        BYTE4   src_Y;  /*   source coordinate Y */
        BYTE4   grp_X;  /*   group coordinate X */
        BYTE4   grp_Y;  /*   group coordinate Y */
        BYTE2   cor_unit;       /*   coordinate unit 1 : length  2 : second */
        BYTE2   weath_vel;      /*   weathering velocity   */
        BYTE2   sweath_vel;     /*   sub-weathering velocity   */
        BYTE2   upht_src;       /*   uphole time at source  */
        BYTE2   upht_grp;       /*   uphole time at group  */
        BYTE2   stcor_src;      /*   source static correction  */
        BYTE2   stcor_grp;      /*   group static correction  */
        BYTE2   st_cor; /*   total static applied  */
        BYTE2   lag_A;  /*   lag time A  */
        BYTE2   lag_B;  /*   lag time B  */
        BYTE2   delay;  /*   delay recording time  */
        BYTE2   mute_start;     /*    mute time - start  */
        BYTE2   mute_end;       /*    mute time - end  */
        BYTE2   nb_samples;     /*   number of samples in the trace  */
        BYTE2   sampling;       /*   sample interval in micro-seconds */
        BYTE2   gain_type;      /*   gain type of field instrument  */
        BYTE2   inst_gain;      /*   instrument gain constant  */
        BYTE2   init_gain;      /*   initial gain   */
        BYTE2   correlated;     /*   correlated  1 : no  2 : yes   */
        BYTE2   swp_start;      /*   sweep frequency at start  */
        BYTE2   swp_end;        /*   sweep frequency at end  */
        BYTE2   swp_length;     /*   sweep length     */
        BYTE2   swp_type;       /*   sweep type 1 : linear 2 : parabolic
                                                3 : exponential 4 :other */
        BYTE2   swp_tap_st;     /*   sweep trace taper length at start */
        BYTE2   swp_tap_ed;     /*   sweep trace taper length at end */
        BYTE2   taper_type;     /*   taper type 1 : linear 2 : cos 3 : other*/
        BYTE2   alias_freq;     /*   alias filter frequency  */
        BYTE2   alias_slope; /*   alias filter slope   */
        BYTE2   notch_freq;     /*   notch filter frequency  */
        BYTE2   notch_slope;    /*   notch filter slope   */
        BYTE2   low_cut;        /*   low cut frequency   */
        BYTE2   high_cut;       /*   high cut frequency   */
        BYTE2   low_slope;      /*   low cut slope   */
        BYTE2   high_slope;     /*   high cut slope  */
        BYTE2   year_of_rec; /*   year data recorded  */
        BYTE2   day_of_rec;     /*   day of year  */
        BYTE2   hour_of_rec;    /*   hour of day  */
        BYTE2   mn_of_rec;      /*   minute of hour  */
        BYTE2   scnd_of_rec;    /*   second of minute  */
        BYTE2   time_basis;     /*   time basis code 1 : local 2 : GMT 
                                                3 : other   */
        BYTE2   tr_weigth;      /*   trace weigthing factor   */
        BYTE2   gnb_roll_one;/*    geophone group number of roll switch
                                        position one  */
        BYTE2   gnb_tr_one;     /*   geophone group number of first trace 
                                within original field record  */
        BYTE2   gnb_tr_last;    /*   geophone group number of last trace
                                within original field record  */
        BYTE2   gap_size;       /*   gap size ( total of groups dropped ) */
        BYTE2   overtravel;     /*   overtravel associated with taper */
        BYTE2   maxtr;
        BYTE2   dummy;
        BYTE4   statnu_mid;
        BYTE4   statnu_so;
        BYTE4   statnu_rec;
        BYTE2   line_nu;
        BYTE2   dummy1;
        BYTE4   sp_nu;
        BYTE4   wat_bot_mid;
        BYTE4   line_nu2;
        BYTE4   sp_nu2;
        BYTE4   X_mid;
        BYTE4   Y_mid;
        BYTE4   X_s;
        BYTE4   Y_s;
        BYTE4   X_g;
        BYTE4   Y_g;
        char    unass[60];      /*   unassigned data  */
}  SEGY_TR_HD;

#define  LG_SEGY_TR_HD  sizeof(SEGY_TR_HD)

static unsigned short sample_size[] = { 4, 4, 2, 2, sizeof(float) };

#define  DIFF(h1,h2,part)  \
        (bcmp((h1)->part, (h2)->part, sizeof((h1)->part)))

#define  MAX_SIZE  40244


#define  isprint(c) \
        ( (c) > 037 && (c) < 0176 )
#define  HEX(c)  \
        (c =( c < 10 ) ? c+'0' : c-10+'A')

static int hex_dmp(zone, size, file)
char    *zone;
int     size;
FILE    *file;
{
        int     i,j,k, nb_line, nb_prt, nb_byte, il;
        char    l1[100], l2[100];

        nb_byte = sizeof(int);
        nb_prt = 4;
        nb_line = size/(nb_byte*nb_prt)+1;

        fprintf(file, " DUMP at %x\n", zone);
        for( i = 0 ; nb_line > 0 && i < size ; nb_line-- )  {
                for( j = 0 , il = 0 ; j < nb_prt && i < size ; j++ )  {
                        for( k = 0 ; k < nb_byte && i < size ; k++ )  {
                                l1[il] = (zone[i]>>4)&017;
                                HEX(l1[il]);
                                l2[il++] = ' ';
                                l1[il] = zone[i]&017; 
                                HEX(l1[il]);
                                l2[il++] = (isprint(zone[i])) ? zone[i]: ' ';
                                i++;
                                }
                        l1[il] = ' ';
                        l2[il++] = ' ';
                        }
                l1[il] = '\0';
                l2[il] = '\0';
                fprintf(file,"%s\n",l1);
                fprintf(file,"%s\n",l2);
                }

        
        return;
}

#ifndef S_ISCHR
#define S_ISCHR(st) ((st)&S_IFCHR)
#endif

#define DUMP_FIELD(fld, cmt) \
  { int v = 0; v = ntohl(fld); fprintf(fout, "%s : %d\n", cmt, v ); }

#define DUMP_FIELD_SHORT(fld, cmt) \
  { int v = 0; v = ntohs(fld); fprintf(fout, "%s : %d\n", cmt, v ); }

#define DUMP_FIELD_CASE(fld, cmt, arr, amin, amax) \
  { int v; v = ntohs(fld);						   \
      fprintf(fout, "%s : (%d)", cmt, v); \
          if( v >= amin && v <= amax ) fprintf(fout, " %s\n", arr[v]); \
            else fprintf(fout, " Unknown code\n");}

static void dump_segy_hd(hd, fout)
SEGY_HD *hd;
FILE *fout;
{
    static char *fmt_arr[] = {0, "floating point", "fixed_point(4 bytes)",
                       "fixed point(2 bytes)", "fixed point with gain code"};
    static char *sweep_arr[] = { 0, "linear", "parabolic", "exponential", "other"};
    static char *taper_arr[] = { 0, "linear", "cos", "other"};
    static char *bool_arr[] = {0, "no", "yes"};
    static char *meas_arr[] = {0, "meters", "feet"};
    static char *recov_arr[] = {0, "none", "spherical divergence", "AGC", "other"};

    
    DUMP_FIELD(hd->job_number, "job identification number");
    DUMP_FIELD(hd->line_number,"line number");
    DUMP_FIELD(hd->reel_number, "reel number");
    DUMP_FIELD_SHORT(hd->nb_tra_rec, "number of traces per record");
    DUMP_FIELD_SHORT(hd->nb_aux_rec, "number of auxiliary traces per record");
    DUMP_FIELD_SHORT(hd->sampling, "sampling interval ( for the reel )");
    DUMP_FIELD_SHORT(hd->sampl_fld, "sampling interval ( for original rec. )");
    DUMP_FIELD_SHORT(hd->nb_samples, "number of samples ( for the reel ) ");
    DUMP_FIELD_SHORT(hd->nb_samp_fld, "number of samples ( for original rec.)");
        
    DUMP_FIELD_CASE(hd->data_form, "data format", fmt_arr, 1, 4);
    DUMP_FIELD_SHORT(hd->cdp_fold, "CDP fold");
    DUMP_FIELD_SHORT(hd->trace_sort, "trace sorting ");
    DUMP_FIELD_SHORT(hd->vert_sum, "vertical sum");
    DUMP_FIELD_SHORT(hd->swp_start, "sweep frequency at start ");
    DUMP_FIELD_SHORT(hd->swp_end, "sweep frequency at end ");
    DUMP_FIELD_SHORT(hd->swp_length, "sweep length    ");
    DUMP_FIELD_CASE(hd->swp_type, "sweep type", sweep_arr, 1, 4);
    DUMP_FIELD_SHORT(hd->swp_channel, "trace number of sweep channel ");
    DUMP_FIELD_SHORT(hd->swp_tap_st, "sweep trace taper length at start");
    DUMP_FIELD_SHORT(hd->swp_tap_ed, "sweep trace taper length at end");
    DUMP_FIELD_CASE(hd->taper_type, "taper type", taper_arr, 1, 3);
    DUMP_FIELD_CASE(hd->correlated, "correlated", bool_arr, 1, 2);
    DUMP_FIELD_CASE(hd->bin_gain, "binary gain recovered", bool_arr, 1, 2);
    DUMP_FIELD_CASE(hd->amp_recover, "amplitude recovery method", recov_arr,
                    1, 4);
    DUMP_FIELD_CASE(hd->meas_sys, "measurement system", meas_arr, 1, 2);
    DUMP_FIELD_SHORT(hd->polarity, "impulse signal polarity ");
    DUMP_FIELD_SHORT(hd->vib_pol, "vibratory polarity");

}

static char buf[MAX_SIZE], out_buf[MAX_SIZE];
static float fb[3000];
static int nb_tr = 0;
static char *multiple_file, *multiple_host;
static char dev_name[510];

int mygetopt(argc, argv, opt, val)
int argc;
char *argv[], *opt, *val;
{
    int i = 1;
    int lg_opt = strlen(opt);
    
    for( i = 1 ; i < argc ; i++ ) {
        char *arg = argv[i];
        if( !strncmp(arg, opt, lg_opt) ) {
            if( lg_opt == strlen(arg) )
                strcpy(val, argv[i+1] ? argv[i+1] : "");
            else
                strcpy(val, arg+lg_opt);
            return i;
        }
    }
    return 0;
}

/* Read all the file */

void open_multiple()
{
    char *pp, *p = (char*)malloc(strlen(buf));
    strcpy(p, buf+1);
    pp = strchr(p, ':');
    if( pp ) {
	multiple_host = p;
	*pp = 0;
	p = pp+1;
    }
    multiple_file = p;
    buf[0] = 0;
}

FILE *next_file(int v)
{
    FILE *file;
    static FILE *prev_file = 0;
    static int idx = 1;
    static int prev_v = 0;
    v = prev_v;
    if( prev_v == v ) 
	v = idx++;
    else 
	prev_v = v;
    if( multiple_host == 0 ) {
	char buffer[100];
	if( multiple_file[0] == 0 )
	    return 0;
	if( multiple_file[0] == '-' && multiple_file[1] == 0 )
	    file = stdout;
	else {
	    sprintf(buffer, "%s-%d", multiple_file, v);
	    if( prev_file )
		fclose(prev_file);
	    file = fopen(buffer, "w");
	}
    }
    else {
	char buffer[200];
	sprintf(buffer, "rsh %s dd of=%s-%d", multiple_host, multiple_file, v);
	if( prev_file )
	    pclose(prev_file);
	file = popen(buffer, "w");
    }

    prev_file = file;
    return file;
}

static void skip_bytes(file, lg)
FILE *file;
int lg;
{
    char buf[512];
    while( lg > 0 ) {
	int nb = fread(buf, lg < 512 ? lg : 512, 1, file);
	lg -= nb;
    }
}

static int read_block(file, buf, sz)
FILE *file;
char *buf;
int sz;
{
    int lg;
    char str[9];
    fread(str, 8, sizeof(char), file);
    str[8] = 0;
    sscanf(str, "%d", &lg);
    if( sz > lg ) {
	fread(buf, lg, sizeof(char), file);
	return lg;
    }
    else {
	fread(buf, sz, sizeof(char), file);
	skip_bytes(file, lg-sz);
	return sz;
    }
}

static int cdp_min = 0, cdp_max = 0;

static int trace_is_in_area(tr_hd)
SEGY_TR_HD *tr_hd;
{
    if( cdp_min >= cdp_max )
	return 1;
    if( ntohl(tr_hd->cdp_ens) <= cdp_min )
	return 0;
    if( ntohl(tr_hd->cdp_ens) >= cdp_max )
	return 0;
    return 1;
}

#define USAGE \
" %s -i <input> [ -o output ] [ -dump ]\n\
   -o name : the input is checked and copied to name\n\
   the input or output can be <hostname>:<file-name>\n\
   -dump : dump the binary header\n\
   -no_headers : Don't write tape headers (3200 &400 bytes)\n\
   -format [ ibm or integer ] : Transform data in ibm or 4 bytes integer\n\
   -dump_sp <file_name> : Dump traces headers\n\
   -no_error_hd : Don't print errors on missing tape headers\n\
   -cdp_min : Write the trace that belongs to the given interval.\n\
                  The test is done on word 6.\n\
   -cdp_max : Write the trace that belongs to the given interval.\n\
                  The test is done on word 6.\n\
   -max_traces : maximum number of traces in output \n\
   -skip_traces : number of traces to skip before writing the output \n\
   -split_output : maximum number of traces of each part. The name of each\n\
     part is build from the original name by incrementing the last character.\n\
     Make sure that the file name given in -o option finishes by '0'.\n\
   -split_hd : option used to write a segy header at the beginning of \n\
     each splitted output file \n\
   -all : Skip end of file on read. Useful to read a tape containing \n\
     multiple files in one run.\n\
   -cov [file off1 sz1 ...] : extract coverage in 'file'.\n\
     The coverage file will be written in 'file', seven values per trace.\n\
     Each value is defined by its offset in byte ( beginning at 0 )\n\
     and the size ( 2 for 2byte integer and 4 for 4bytes integer )\n\
     All these arguments must be enclosed in \"\n\
     Version 2013.12.3 Please contact Bill Menger for help\n"

        
#define READ(file, buf, size) \
( is_blocked ? read_block(file, buf, MAX_SIZE) : \
  (is_tape ? read_tape(fileno(file), buf, MAX_SIZE) : fread(buf, 1, size, file)) )

static int write_and_check(FILE * file, char * buf, size_t size);
#define CHECK_SPLIT(file) if( split_output > 0 && nb_written_traces >= nb_split_to_write ) file = new_file_for_split(file);

static SEGY_HD segy_hd;
static char ebcdic_hd[3200];
static short nb_samples, data_format, byte_per_sample;
static size_t  lg_tr;
static int dump_hd, is_tape = 0, is_blocked = 0, no_headers = 0;
static int output_is_tape = 0;
static int nb_written_traces = 0;
static int max_written_traces = -1;
static int skip_tr = 0;
static short output_fmt = -1;
static char error_hd = 0;
static void (*check_trace)(); /* Check each trace in a peculiar manner */
static int all_files_in_input = 0;
static int quiet = 0;
static int split_output = -1;
static int nb_split_to_write = -1;
static int split_hd = 0;
static char ext_ebcdic_file[100];
static int cdpfirst = -1;
static int cdplast = -1;

static FILE *new_file_for_split(FILE *file)
{
    if( output_is_tape ) {
	// Goto next tape.
	int fd = fileno(file);
	int itest, ret;
	/* Write two EOFS */
	struct mtop mtc;
	struct mtop mt_command;
	mtc.mt_op = MTWEOF;
	mtc.mt_count = 2;
	if( ioctl(fd, MTIOCTOP, &mtc) == -1 )
	    perror("ioctl");
	fprintf(stderr, "Ejecting current tape\n");
	mt_command.mt_op = MTOFFL;
	ret = ioctl(fd, MTIOCTOP, &mt_command);
	fclose(file);
	itest = 0;
	again_split :
	  /* try to open a new tape : stacker */
	  do {
	    sleep(5);
	    file = fopen(dev_name, "w");
	    ++itest;
	} while (file == 0 && itest < 100 );
	if (file == 0 && itest >= 100 ){
#if 0
	  fprintf(tty, "Cannot find a new tape, insert new tapes\n");
	  fflush(tty);
	  rewind(tty);
	  fgets(buf, 100, tty);
	  lgs = strlen(buf);
	  if( buf[lgs-1] == '\n' )
	    buf[lgs-1] = 0;
	  goto again_split;
#endif
	  exit(1);
	}
	if( file == 0 ) {
	    fprintf(stderr, "Cannot open device %s, sorry ...\n", dev_name);
	    exit(1);
	}
    }
    else {

	char *p = dev_name+strlen(dev_name)-1;
	(*p)++;
	fclose(file);
	file = fopen(dev_name, "w");
    }
    nb_split_to_write += split_output;
    if ( split_hd ){
	fprintf( stderr,"split_d %d \n", split_hd);
	write_and_check(file, ebcdic_hd, (size_t) 3200);
	write_and_check(file, (char *) &segy_hd, (size_t) 400);
    }
    return file;
}

static int read_tape(fd, buf, lg)
int fd;
char *buf;
int lg;
{
    int retry = 0;
    int nb = read(fd, buf, lg);
    if( nb == 0 && all_files_in_input ) {
	/* Skip to next file */
	struct mtop mt;
	mt.mt_op = MTFSF;
	mt.mt_count = 1;
	if( ioctl(fd, MTIOCTOP, &mt) != 0 ) {
	  perror("ioctl");
	  return -1; 
	}
	nb = read(fd, buf, MAX_SIZE);
	if( nb == 3200 ) {
	  fprintf(stderr, "New file, read succesfully 3200 bytes\n");
	    nb = read(fd, buf, MAX_SIZE);
	    if( nb == 400 ){
	      fprintf(stderr, "New file, read succesfully 400 bytes\n");
	      nb = read(fd, buf, lg);
	      if ( nb < 0 ) perror( "read_tape" );
	    }
	}
    }

    while ( nb < 0 && retry < 100 ){
      	/* Skip to next file */
	struct mtop mt;
	mt.mt_op = MTFSR;
	mt.mt_count = 5;
	if( ioctl(fd, MTIOCTOP, &mt) != 0 ) {
	  perror("ioctl FSR");
	  return -1;
	}
	fprintf(stderr, "Successfully Skipped 5 records\n");
	nb = read(fd, buf, lg);
	retry++;
    }
    return nb;
}

void change_buf(in,nb)
int *in, nb;
{
in[0] = nb;
  }

static int write_and_check(file, buf, lg)
FILE *file;
char *buf;
size_t lg;
{
    struct stat st;
    struct mtop mt_command;
    int nb, fd = fileno(file);
    if( DEBUG) fprintf(stderr,"%d: FILE=%d buf=%X lg=%d\n",__LINE__,fd,buf,lg);

    /* Change the trace sequence number within reel */
    if ( lg != 3200 && lg != 400 ){
SEGY_TR_HD *tr_tmp_hd = (SEGY_TR_HD*)buf;
int test_trace=ntohl(tr_tmp_hd->traseqlin);   
int test_trace2=tr_tmp_hd->traseqrel; /* ntohl(tr_tmp_hd->traseqrel); */
int test_trace3=ntohl(tr_tmp_hd->tr_in_cdp);
if(DEBUG)fprintf(stderr, "%d:    traseqlin %d, traseqrel %d tr_in_cdp %d\n",__LINE__, test_trace,test_trace2,test_trace3 );
if(DEBUG)fprintf(stderr, "%d: BS traseqlin %d, traseqrel %d tr_in_cdp %d\n",__LINE__, tr_tmp_hd->traseqlin,tr_tmp_hd->traseqrel,tr_tmp_hd->tr_in_cdp );
      nb_tr++;
      change_buf(buf+4,htonl(nb_tr));
      test_trace2=ntohl(tr_tmp_hd->traseqrel);
      if(DEBUG)fprintf(stderr, "%d:    traseqlin %d, traseqrel %d tr_in_cdp %d\n",__LINE__, test_trace,test_trace2,test_trace3 );
      if(DEBUG)fprintf(stderr,"%d: nb_tr=%d\n",__LINE__,nb_tr);
    } else {
      nb_tr=0;
    }


    if( output_is_tape == 0 ) {
      nb = fwrite(buf, 1, lg, file);
      if( nb == lg )
	return nb;
      else {
	perror("write");
	return -1;
      }
    }
    /* if here, then output_is_not_tape!!! */
    nb = write(fd, buf, lg);
    if( nb == lg )
      return nb;
    if(DEBUG)fprintf(stderr,"%d: nb=%d lg=%d\n",__LINE__,nb,lg); 
    fstat(fd, &st);
    if( S_ISCHR(st.st_mode) ) {
	/* try to reopen a tape */
	FILE *tty = fopen("/dev/tty", "w+");

	if( tty ) {
	    int lgs, new_fd;
	    int itest, ret;
	    char buf[100];
	    /* Write two EOFS */
	    struct mtop mtc;
	    mtc.mt_op = MTWEOF;
	    mtc.mt_count = 2;
	    if( ioctl(fd, MTIOCTOP, &mtc) == -1 )
		perror("ioctl");
	    fprintf(stderr, "Ejecting current tape\n");
	    mt_command.mt_op = MTOFFL;
	    ret = ioctl(fd, MTIOCTOP, &mt_command);
	    close(fd);
	    again :
	    itest = 0;
	    /* try to open a new tape : stacker */
	    do {
	      sleep(5);
	      new_fd = open(dev_name, O_WRONLY);
	      ++itest;
	    } while (new_fd<0 && itest<100);

	    if ( itest >= 100 ){
	      fprintf(tty, "Cannot find a new tape, insert new tapes\n");
	      fflush(tty);
	      rewind(tty);
	      fgets(buf, 100, tty);
	      lgs = strlen(buf);
	      if( buf[lgs-1] == '\n' )
		buf[lgs-1] = 0;
	      goto again;
	    }
	    write(fd, ebcdic_hd, 3200);
	    write(fd, &segy_hd, sizeof(segy_hd));
	    nb_tr = 1;
	    change_buf(buf+4,nb_tr);
            if(DEBUG)fprintf(stderr,"%d: %d\n",__LINE__,nb_tr);
	    write(fd, buf, lg);
	    fclose(tty);
	    return lg;
	}
    }
    return -1;
}

int read_a_tape(fdin, fdout, file_info, tape_number, file_dump_sp)
FILE *fdin, *fdout;
FILE *file_info;   /* Dump informations/errors on this files */
int tape_number;
FILE *file_dump_sp; /* Dump trace header info on this file */
{
    size_t lg_tr_out,lg_tr;
    int nb, lg_read, nb_samples_error = 0;
    int skip_read = 0;
    int current_trace = 1;
    int try_count=0;

    /*  Read the EBCDIC Header */
    
    lg_read = is_tape ? MAX_SIZE : 3200;
    nb = READ(fdin, buf, 3200);
    if( nb == 0 )
	return -1;

    if( nb != 3200 ) {
        if( error_hd ) {
	    fprintf(file_info, "EBCDIC Header not of size 3200\n");
	    fprintf(file_info, " ebcdic header %d \n\n", nb);
	    hex_dmp(ebcdic_hd, 160, file_info);
	}
        if( nb == - 1) {
          perror("cp_segy");
        }
    }
    else
	memcpy(ebcdic_hd, buf, 3200);

    /* If an external ebcdic exists, read it now */

    if( ext_ebcdic_file[0] != 0 ) {
	int l;
	FILE *f = fopen(ext_ebcdic_file, "r");
	l = fread(ebcdic_hd, 1, 3200, f);
	fclose(f);
	if( l != 3200 ) {
	    fprintf(stderr, "EBCDIC File %s has not correct length ?\n", ext_ebcdic_file);
	    exit(1);
	}
    }
	

    /*  Read the binary header */
    
    if( nb == 400 ) {
        fprintf(stderr, " Previous header was 400, Try to use it as binary header\n");
        if( tape_number == 1 )
            write_and_check(fdout, ebcdic_hd, (size_t) 3200);
    }
    else if( nb == 3200 ) 
        nb = READ(fdin, buf, 400);

    if( nb != 400 ) {
        if( nb == -1 )
                perror("Binary header");
        else if( error_hd ) {
	    fprintf(file_info,
		    " Binary Header not  of size 400 ( actually %d )\n",
                    nb);
	    hex_dmp(buf, 400, file_info);
        }
	skip_read = 1;
    }
    else {
	memcpy(&segy_hd, buf, 400);
        if( dump_hd )
            dump_segy_hd(&segy_hd, file_info);
    }

    /* Write both headers */
    data_format = ntohs(segy_hd.data_form);
    if( output_fmt != -1 )
      segy_hd.data_form = htons(output_fmt);    
    if (DEBUG) fprintf(stderr,"%d: Output_format=%d\n",__LINE__,output_fmt);
    if( multiple_file )
	  fdout = next_file(ntohl(segy_hd.line_number));
    if( fdout != 0 && no_headers == 0 && tape_number == 1 )
	write_and_check(fdout, ebcdic_hd, (size_t) 3200);
    if( fdout != 0 && no_headers == 0 && tape_number == 1 )
	write_and_check(fdout, (char *) &segy_hd, (size_t) 400);

    
    /* If no copy, exit now */

    if( fdout == 0 && check_trace == 0 )
        return 0;
    
    /*  Compute trace length */
    
    if( tape_number == 1 ) {
	  nb_samples = ntohs(segy_hd.nb_samples);
        if (DEBUG) fprintf(stderr,"%d: nb_samples=%d\n",__LINE__,nb_samples);
	/*        data_format = segy_hd.data_form; */
	fprintf( stderr, " data_format : %d\n", data_format );
        byte_per_sample = 4;
        if( data_format == 3 ) byte_per_sample = 2;
        if( output_fmt != -1 ) {
            lg_tr_out = 240 + nb_samples * 2;
            if( output_fmt != 3 )
                lg_tr_out += nb_samples * 2;
            segy_hd.data_form = htons(output_fmt);
        }
    }
    else {
        short nbs, dtf;
        nbs = ntohs(segy_hd.nb_samples);
        if(DEBUG) fprintf(stderr,"%d: nbs=%d\n",__LINE__,nbs);
        dtf = ntohs(segy_hd.data_form);
        if(DEBUG)fprintf(stderr,"%d: dtf=%d\n",__LINE__,dtf);
        if( nbs != nb_samples ) 
            fprintf(file_info,
"Error for tape %d : number of samples not corresponding %d ( expected %d )\n",
                    tape_number, nbs, nb_samples);
        if( dtf != data_format )
            fprintf(file_info,
"Error for tape %d : data format not corresponding %d ( expected %d )\n", 
                    tape_number, nbs, nb_samples);
    }

    lg_tr = 240+nb_samples*byte_per_sample;
    if (DEBUG) fprintf(stderr,"%d: lg_tr=%d\n",__LINE__,lg_tr);

    /*  Read in a trace, check its length and write it */

    //    fprintf(stderr, "skip_read %d\n", skip_read);
#if 0
    // IDB's mess
        nb=READ(fdin, buf,8);
        while (1) {
          SEGY_TR_HD *tr_tmp_hd = (SEGY_TR_HD*)buf;
          int test_trace=ntohl(tr_tmp_hd->traseqlin);
          int test_trace2=ntohl(tr_tmp_hd->traseqrel);
	  if(DEBUG)fprintf(stderr, "traseqlin %d, traseqrel %d\n",test_trace,test_trace2 );
          if (test_trace == 1 ) goto start_trace;
          nb=READ(fdin, buf,8);
        }


    start_trace :
      current_trace++;
      while( skip_read || ( nb = READ(fdin, buf+8, lg_tr-8 ) ) > 0 ) { 
#endif

      while( skip_read || ( nb = READ(fdin, buf, lg_tr ) ) > 0 ) { 
        SEGY_TR_HD *tr_hd = (SEGY_TR_HD*)buf;
        short tr_nb_samples = ntohs(tr_hd->nb_samples);

		int w = -(ntohs(tr_hd->tr_weigth));
	float weight = pow(2.0, (double)w);

	skip_read = 0;
	if(DEBUG)fprintf(stderr, "Number of samples %d\n", tr_nb_samples);

        if( nb_samples_error < 5 && tr_nb_samples != nb_samples ) {
            fprintf(stderr, "Number of samples not correct %d ( expect %d )\n",
                    tr_nb_samples, nb_samples);
            nb_samples_error++;
        }

	/* In any case, set the trace number of samples to the header */
		tr_hd->nb_samples = htons(nb_samples);

        /* Dump sp informations if needed */
        
        if( file_dump_sp ) {
	    union { float v; int i; } res;
            int sp1, esp, ntr, ncdp;
	    int n_field,
	      n_tr ,
	      n_esp ,
	      n_cdp ,
	      n_st ,
	      n_off ,
	      n_xs ,
	      n_ys , 
	      n_xg , 
	      n_yg,
	      n_stnu ,
	      n_stnuso ,
	      n_stnure, 
	      n_line, 
	      n_spnu;
	    float  n_wb,
	      n_line2,
	      n_spnu2,
	      n_xm,
	      n_ym,
	      n_xs2,
	      n_ys2,
	      n_xg2,
	      n_yg2;

	    n_field = ntohl(tr_hd->field_rec);
	    n_tr = ntohl(tr_hd->tracnb_fld);
	    n_esp = ntohl(tr_hd->esp);
	    n_cdp = ntohl(tr_hd->cdp_ens);
	    n_st = ntohs(tr_hd->nbhst);
	    n_off = ntohl(tr_hd->srdist);
	    n_xs = ntohl(tr_hd->src_X);
	    n_ys = ntohl(tr_hd->src_Y);
	    n_xg = ntohl(tr_hd->grp_X);
	    n_yg = ntohl(tr_hd->grp_Y);
	    n_stnu = ntohl(tr_hd->statnu_mid);
	    n_stnuso = ntohl(tr_hd->statnu_so);
	    n_stnure = ntohl(tr_hd->statnu_rec);
	    n_line = ntohs(tr_hd->line_nu);
	    n_spnu = ntohl(tr_hd->sp_nu);
	    n_wb = ntohl(tr_hd->wat_bot_mid);
	    n_line2 = ntohl(tr_hd->line_nu2);
	    n_spnu2 = ntohl(tr_hd->sp_nu2);
	    n_xm = ntohl(tr_hd->X_mid);
	    n_ym = ntohl(tr_hd->Y_mid);
	    /*  n_xs2 = ntohl(tr_hd->X_s); */
	    n_ys2 = ntohl(tr_hd->Y_s);
	    n_xg2 = ntohl(tr_hd->X_g);
	  n_yg2 = ntohl(tr_hd->Y_g);
	    
            fprintf(file_dump_sp, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d  \n", 
		    n_field,
		    n_tr ,
		    n_esp ,
		    n_cdp ,
		    n_st ,
		    n_off ,
		    n_xs ,
		    n_ys , 
		    n_xg , 
		    n_yg ,
                    n_stnu ,
		    n_stnuso ,
		    n_stnure, 
		    n_line, 
		    n_spnu );
/* 	      	    n_wb, */
/* 		    n_line2, */
/* 		    n_spnu2, */
/* 		    n_xm, */
/* 		    n_ym, */
/* 		    n_xs2, */
/* 		    n_ys2, */
/* 		    n_xg2, */
/* 		    n_yg2  */
/* 		    );  */
        }
        
        /* If the read trace is too short, pad it with 0 */

        if( nb < lg_tr ) {
            int i;
            for( i = nb ; i < lg_tr ; i++ )
                buf[i] = 0;
        }

	/* Check if the trace is in the wanted area */

	if( !trace_is_in_area(tr_hd) )
	    continue;

        if( fdout != 0 || check_trace != 0 ) {
            if( output_fmt == -1 ||
		output_fmt == data_format ) {

		if ( skip_tr == 0 ){
		  if ( cdpfirst == -1 ) cdpfirst = ntohl(tr_hd->cdp_ens);
		  cdplast = ntohl(tr_hd->cdp_ens);
		  nb_written_traces++;
		  write_and_check(fdout, buf, (size_t) lg_tr);
		  CHECK_SPLIT(fdout);
		  if( check_trace )
		    (*check_trace)(buf, lg_tr, &segy_hd);
		}
		else
		  skip_tr--;
	    }
            else {  /* output_fmt != data_format */
                bcopy(buf, out_buf, 240);
                switch(output_fmt) {
                    case 1: /* output floating ibm */
                    {
                        switch(data_format) {
                            case 2: /* integer format */
                            {
                                int i, *p = (int*)(buf+240);
                                for( i = 0 ; i < nb_samples ; i++ )
								  fb[i] = ntohl(p[i]);
                                break;
                            }
                            case 3: /* two-bytes format */
                            {
                                short *p = (short*)(buf+240);
                                int i;
				                for( i = 0 ; i < nb_samples ; i++ ) {
				                   int vv = (short)ntohs(p[i]);
				                   fb[i] = ((float)(vv))*weight;
				                }
				                break;
                            }
			                case 5: /* ieee format */
			                {
				                float *p = (float*)(buf+240);
				                int i;
                                for( i = 0 ; i < nb_samples ; i++ )
                                    fb[i] = p[i];
                                break;
                            }
                        }
			
                        /*  convert fb in ibm format */

			            ieee2ibm( fb, out_buf+240, nb_samples);
			            break;
                    }
                    case 2: /* output data integer */
                    {
                        switch(data_format) {
                            case 3: /*  two-bytes integer  */
                            {
                                short *p = (short*)(buf+240);
                                int i, *pp = (int*)(out_buf+240);
                                for( i = 0 ; i < nb_samples ; i++ )
                                    pp[i] = p[i];
                            }
                        }
			            break;
                    }
		            case 5: /* output floating ieee ( native ) */
		            {
			            switch(data_format) {
			                case 1: /* floating point ibm */
			                {
				                /* convert ibm to ieee  */
				
				                ibm2ieee(buf+240, out_buf+240, nb_samples);
				                break;
			                }				
                            case 2: /* integer format */
                            {
                                int i, *p = (int*)(buf+240);
				                float *pf = (float*)(out_buf+240);
                                for( i = 0 ; i < nb_samples ; i++ )
                                    pf[i] = p[i];
                                break;
                            }
                            case 3: /* two-bytes format */
                            {
                                short *p = (short*)(buf+240);
                                int i;
				                float *pf = (float*)(out_buf+240);
                                for( i = 0 ; i < nb_samples ; i++ )
                                    pf[i] = p[i];
                                break;
                            }
                        }
		            }
                }
		if( fdout )
		  if ( skip_tr == 0 ){
		    write_and_check(fdout, out_buf, (size_t) lg_tr_out);
		    nb_written_traces++;
		    CHECK_SPLIT(fdout);
		    if( check_trace )
		      (*check_trace)(out_buf, lg_tr_out, &segy_hd);
		  }
		  else
		    skip_tr --;
            }

	    if( max_written_traces > 0 &&
		nb_written_traces >= max_written_traces)
	      exit(1);
	    /*		return -1; */
        }

#if 0
	// IDB's mess
	try_count=0;
	if ( nb=READ(fdin, buf,8) > 0 ){
	  while (1) {
	    SEGY_TR_HD *tr_tmp_hd = (SEGY_TR_HD*)buf;
	    int test_trace=ntohl(tr_tmp_hd->traseqlin);   
	    int test_trace2=ntohl(tr_tmp_hd->traseqrel);
	    if(DEBUG)fprintf(stderr, "%d: traseqlin %d, traseqrel %d\n",__LINE__, test_trace,test_trace2 );
	    if (test_trace > 0 && test_trace < 100 && test_trace2 == current_trace && try_count > 410 ) goto continue_trace;
	    if ( nb=READ(fdin, buf,8) <= 0) goto finished;
	    try_count++;
	  }
	}
	else 
	  goto finished;
    continue_trace:
	current_trace++;	
#endif
    }

    if( nb < 0 ) {
        perror("Reading Tape");
    }
    finished:
    segy_hd.data_form = htons(data_format);

    return nb;
}

#define SED_STRING \
" |\
sed 's/\\(.*\\)-o[ ]*\\([^ :]*\\):\\([^ ]*\\)\\(.*\\)/\\1 -o - \\4 | \
 rsh \\2 cp_segy -i - -o \\3/\n\
s/\\(.*\\)-i[ ]*\\([^ :]*\\):\\([^ ]*\\)\\(.*\\)/rsh \\2 $CP_SEGY -i \\3 -o - | \
\\1 -i - \\4/'"

void exec_remote(argc, argv)
int     argc;
char    *argv[];
{
    char *str;
    FILE *file;
    int i;
    strcpy(buf, "echo cp_segy ");
    str = buf+strlen(buf);

    for( i = 1 ; i < argc ; i++ ) {
        char *ptr = argv[i];
        while( *ptr )
            *str++ = *ptr++;
        *str++ = ' ';
    }

    strcpy(str, SED_STRING);
    file = popen(buf, "r");
    i = fread(buf, 1, 1000, file);
    pclose(file);
    buf[i] = 0;
    fprintf(stderr, "Going to execute \"%s\"\n", buf);
    execl("/bin/sh", "/bin/sh", "-c", buf, 0);
}


static float cube_dim[3][3];
static FILE *cube_out = 0;

#define grid(x,s) ((int)((x)/(s)+0.5))

static void cube_write(buf, lg, segy_hd)
char *buf;
int lg;
SEGY_HD *segy_hd;
{
    SEGY_TR_HD *tr_hd = (SEGY_TR_HD*)buf;
    int nb_traces = grid(cube_dim[1][1]-cube_dim[0][1], cube_dim[2][1])+1;
    int nb_samp = grid(cube_dim[1][2]-cube_dim[0][2], cube_dim[2][2])+1;
    unsigned long pos;
    int sample_size = 4;
    int line_number = ntohl(tr_hd->grp_X); /* field_rec; segy_hd->line_number;*/
    int tr_number = ntohl(tr_hd->grp_Y); /* tracnb_fld; tr_hd->cdp_ens */
    int beg_trace = grid(cube_dim[0][2], cube_dim[2][2]);

    if( nb_samp > ntohs(tr_hd->nb_samples) )
	  nb_samp = ntohs(tr_hd->nb_samples);

    if( line_number < cube_dim[0][0]
       || line_number > cube_dim[1][0] )
	return;

    if( tr_number < cube_dim[0][1]
       || tr_number > cube_dim[1][1] )
	return;

    if( ntohs(segy_hd->data_form) == 3 )
	sample_size = 2;
    
    pos = grid(line_number-cube_dim[0][0], cube_dim[2][0]);
    pos *= nb_traces;
    pos += grid(tr_number-cube_dim[0][1], cube_dim[2][1]);
    pos *= nb_samp;
    pos *= sample_size;
    fseek(cube_out, pos, SEEK_SET);
    fwrite(buf+sizeof(SEGY_TR_HD)+sample_size*beg_trace, sample_size,
	   nb_samp, cube_out);
}
    
static void setup_cube(buf)
char *buf;
{
    char name[100];
    int nb = sscanf(buf, "%s %f %f %f %f %f %f %f %f %f", name,
		    &cube_dim[0][0], &cube_dim[1][0], &cube_dim[2][0], 
		    &cube_dim[0][1], &cube_dim[1][1], &cube_dim[2][1], 
		    &cube_dim[0][2], &cube_dim[1][2], &cube_dim[2][2]);

    cube_out = fopen(name, "a+");
    check_trace = cube_write;
}

/*
  Coverage 
  */

static float get_value_from_header(char *p, int offset, int sz)
{
    p += offset;
    if( sz == 2 ) {
	short i;
	memcpy(&i, p, 2);
	return (float)i;
    }
    else if( sz == 4 ) {
	int i;
	memcpy(&i, p, 4);
	return (float)i;
    }
    return 0.0;
}

static struct _cov {
    int v[14];
    FILE *file;
} cov;

static void write_coverage(buf, lg, segy_hd)
char *buf;
int lg;
SEGY_HD *segy_hd;
{
    float b[7];
    int i;
    for( i = 0 ; i < 7 ; i++ )
	b[i] = get_value_from_header(buf, cov.v[2*i], cov.v[2*i+1]);

    fwrite(b, sizeof(float), 7, cov.file);
}

static void parse_coverage(char *p)
{
    int i;
    SEGY_TR_HD *tr = 0;
#define OFF(v) ((char*)&tr->v)-(char*)tr

    char *file_name = strtok(p, " ");
    cov.file = fopen(file_name, "w");
    if( cov.file == 0 ) {
	perror("fopen");
	return;
    }

    cov.v[0] = OFF(src_X);
    cov.v[1] = sizeof(tr->src_X);
    cov.v[2] = OFF(src_Y);
    cov.v[3] = sizeof(tr->src_Y);
    cov.v[4] = OFF(grp_X);
    cov.v[5] = sizeof(tr->grp_X);
    cov.v[6] = OFF(grp_Y);
    cov.v[7] = sizeof(tr->grp_Y);

    for( i = 0 ; i < 14 ; i++ ) {
	char *num = strtok(0, " ");
	if( num == 0 )
	    break;
	cov.v[i] = atoi(num);
    }
	     
    check_trace = write_coverage;
}

static char *multiple_input = 0;
static int range = 0;

FILE *open_multiple_input()
{
    FILE *file;
    char buffer[600];
    range++;
    sprintf(buffer, "%s%d", multiple_input, range);
    file = fopen(buffer, "r");
    return file;
}

main(argc,argv)
int     argc;
char    *argv[];
{

    int tape_number = 1, in_remote = 0;
    FILE *fdin, *fdout;
    struct stat bstat;
    FILE *file_info = stdout, *tty, *file_dump_sp = 0;
    char input_name[500];

    if( argc == 1 ) {
        fprintf(stderr, USAGE, argv[0]);
        exit(0);
    }
    
    /* Check size of BYTE2 and BYTE4 */

    if( sizeof(BYTE2) != 2 || sizeof(BYTE4) != 4 ) {
        fprintf(stderr, "Bad configuration");
        exit(1);
    }

    /*  Open the input file */
    
    if( mygetopt(argc, argv, "-i", buf) == 0 ) {
        fprintf(stderr, "Need at least input\n");
        fprintf(stderr, USAGE, argv[0]);
        exit(1);
    }

    strcpy(input_name, buf);
    if( strchr(input_name, ':') )
        exec_remote(argc, argv);

    /*  Open output file */

    fdout = 0;
    if( mygetopt(argc, argv, "-o", buf) ) {
	if( buf[0] == '+' )
	    open_multiple();
	else if( strchr(buf, ':') )
            exec_remote(argc, argv);

        if( buf[0] == '-' ) {
            fdout = stdout;
            file_info = stderr;
        }
        else if( buf[0] ) {
            fdout = fopen(buf, "w");
	    strcpy(dev_name, buf);

            if( fdout == 0 ) {
                perror(buf);
                exit(1);
            }
        }

        fstat(fileno(fdout), &bstat);
        if( S_ISCHR(bstat.st_mode) )
            output_is_tape = 1;
    }

    if( input_name[0] == '-' ) 
        fdin = stdin;
    else if( input_name[0] == '+' ) {
	/* Multiple File in input */
	multiple_input = input_name+1;
	fdin = open_multiple_input();

	if( fdin == 0 )  {
            perror(input_name);
            exit(1);
        }

        fstat(fileno(fdin), &bstat);
        if( S_ISCHR(bstat.st_mode) )
            is_tape = 1;
    }
    else {
        fdin = fopen(input_name, "r");

        if( fdin == 0 )  {
            perror(input_name);
            exit(1);
        }

        fstat(fileno(fdin), &bstat);
        if( S_ISCHR(bstat.st_mode) )
            is_tape = 1;
    }

   /* Get some options */

    mygetopt(argc, argv, "-ebcdic_file", ext_ebcdic_file);

    if( mygetopt(argc, argv, "-max_traces", buf) )
	max_written_traces = atoi(buf);

    if( mygetopt(argc, argv, "-split_output", buf) ) {
      if( dev_name[0] == 0 ) 
	fprintf(stderr, "Must give an ouptut file for split output\n");
      else
	split_output = nb_split_to_write = atoi(buf);
    }

    if( mygetopt(argc, argv, "-split_hd", buf) ) 
      split_hd = 1;

    if( mygetopt(argc, argv, "-skip_traces", buf) ){
      skip_tr = atoi( buf);
      fprintf( stderr, "skip %d traces\n", skip_tr);
    }

    if( mygetopt(argc, argv, "-no_error_hd") )
	error_hd = 0;
    else 
	error_hd = 1;
    
    if( mygetopt(argc, argv, "-format", buf) ) {
        if( !strcmp(buf, "ibm") ) 
            output_fmt = 1;
        else if( !strcmp(buf, "integer") ) 
            output_fmt = 2;
        else if( !strcmp(buf, "ieee") ) 
            output_fmt = 5;
        else 
            fprintf(stderr, " Output format %s not supported\n", buf);
    }

    if( mygetopt(argc, argv, "-all", buf) ) 
        all_files_in_input = 1;

    if( mygetopt(argc, argv, "-quiet", buf) ) 
        quiet = 1;

    if( mygetopt(argc, argv, "-cube", buf) ) 
        setup_cube(buf);

    if( mygetopt(argc, argv, "-cov", buf) )
	parse_coverage(buf);

    is_blocked = mygetopt(argc, argv, "-blocked", buf);
    dump_hd = mygetopt(argc, argv, "-dump", buf);
    no_headers = mygetopt(argc, argv, "-no_headers", buf);
    if( mygetopt(argc, argv, "-dump_sp", buf) ) {
        file_dump_sp = fopen(buf, "w");
        fprintf(file_dump_sp, 
"Index in reel, fiel record number, energy source point, trace number in field record, cdp number\n");
    }
    
    if( mygetopt(argc, argv, "-cdp_min", buf) )
	sscanf(buf, "%d %d", &cdp_min );

    if( mygetopt(argc, argv, "-cdp_max", buf) )
        sscanf(buf, "%d %d", &cdp_max );

    tty = fopen("/dev/tty", "w");
    if( tty == 0 )
	tty = stderr;

    do {
        int st = read_a_tape(fdin, fdout, file_info, tape_number, file_dump_sp);
	if( multiple_input != 0 ) {
	    FILE *next_file = open_multiple_input();
	    if( next_file ) {
		fclose(fdin);
		fdin = next_file;
		buf[0] = 'Y';
		tape_number++;
	    }
	    else
		buf[0] = 'N';
	}
	else if( multiple_file || max_written_traces > 0 ) {
	    buf[0] = st == -1 ? 'N' : 'Y';
	}
	else {
	    fclose(fdin);

	    if( is_tape && quiet==0 ) {
		do {
		    fprintf(tty, "Do you have another tape ? \n(Mount it if any then type  Y or N ) ");
		    fflush(tty);
		    gets(buf);
		    if( buf[0] == 'y' ) buf[0] = 'Y';
		    if( buf[0] == 'n' ) buf[0] = 'N';
		} while( buf[0] != 'Y' && buf[0] != 'N' );

		if( buf[0] == 'Y' ) {
		    fdin = fopen(input_name, "r");
		    if( fdin == 0 ) {
			perror(buf);
			exit(1);
		    }
		    tape_number++;
		}
	    }
	    else {
	      buf[0] = 'N';
	    }
	}
    } while( buf[0] == 'Y' );

    fprintf( stdout, "Total Number of traces output %d\n", nb_written_traces);
    fprintf( stdout, "first cdp ensemble output %d\n", cdpfirst);
    fprintf( stdout, "last cdp ensemble output %d\n", cdplast);
    
    if( fdout )
	fclose(fdout);

    if( cube_out )
	fclose(cube_out);

    if( cov.file )
	fclose(cov.file);

    exit(0);
}
