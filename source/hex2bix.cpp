#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <time.h>
#include <ctype.h>
#include <assert.h>
#include <docopt/docopt.h>
#include <iostream>
 
#pragma pack (1)

#define ERR_INVALID_CL      "Invalid command line."
#define ERR_INFILE          "Unable to open input file."
#define ERR_OUTFILE         "Unable to create output file."
#define ERR_INVALID_FILE    "The input file is not a valid hex file."
#define ERR_BAD_CHKSUM      "The input file contains a bad checksum."
#define ERR_INVALID_SWTCH   "Invalid command line switch."
#define ERR_UNRECOGNIZED_FIRSTBYTE "Unrecognized first byte.  Must be even values B0-BA."
#define ERR_FTYPE_MISMATCH "File type mismatch.  Attempted to put code in a codeless PROM type."


#define EXT_LENGTH      4
#define EXT_STRING      ".bix"
#define MEMORY_SIZE             0x8000
#define MEMORY_BUFFER_SIZE      0x10000
#define MEM_FILL        0xaa
#define LINE_LENGTH     500
#define RST_STR_LEN     5
#define EELOADER_SIZE   0x200
#define EELOADER_MSB    0x1e

#define BLCK_CONTINUE   0xa5
#define BLCK_END        0x5a

#define TIME_STR_LEN    80
#define DATE_STR_LEN    80

#define LE(num)         num.byte[0] ^= num.byte[1];\
                        num.byte[1] ^= num.byte[0];\
                        num.byte[0] ^= num.byte[1]


typedef uint8_t  BYTE;
typedef uint16_t WORD;
typedef uint32_t DWORD;

#define MSB(num)        (BYTE)(((WORD)num & 0xff00) >> 8)
#define LSB(num)        (BYTE)((WORD)num & 0x00ff)

typedef union
{
    WORD    w;
    BYTE    byte[2];
}XWORD;

typedef enum
{
    FT_BIX = 0,
    FT_IIC,
    FT_ASM,
    FT_HEX
}FILETYPE;

// B6 format:  Others are a subset of this information, so this structure is used to 
// store the data until it is output.
typedef struct
{
    BYTE    prom_type;
    XWORD   VendorID;
    XWORD   FnProductID; 
    XWORD   FnVersionID;
    BYTE    Config0; 
    BYTE    Reserved; 
}IIC_HDR;

typedef struct
{
    XWORD   length;
    XWORD   start;
}BLOCK_HDR;

DWORD   MemSize = MEMORY_SIZE;
BYTE    *Image;
BYTE    *ImageMap;
std::string  InFilename;
std::string  OutFilename;
char    *SymbolName;
IIC_HDR IIC_Hdr = { 0xB2, 0x0547, 0x2131, 0x0000, 0x04, 0x00 };
// IIC_HDR  IIC_Hdr = { 0xBA, 0x547, 0x2504, 0x0000, 0x1002, 0x0000, 0x061F }; //TPM
#define DOWNLOAD_PROM_TYPE (IIC_Hdr.prom_type & 2)
char    Extension[3][5] = { ".bix", ".iic", ".a51" };
BYTE    ResetString[] = { 0x80, 0x01, 0x7f, 0x92, 0x00 };
BYTE    Fx2ResetString[] = { 0x80, 0x01, 0xE6, 0x00, 0x00 };
bool    IIC_Reset = false;
bool    compressIIC = false;
bool    EELoader = false;
//BOOL  IIC_Reset = TRUE; //TPM: Must generate Reset String.

FILETYPE    OutFileType = FT_BIX;
FILETYPE    InFileType = FT_HEX;

// This is a hand-modified version of the file includeme.c created by the eeloader
// Keil project.  The eeloader.c file will contain an 8 byte header that must be removed.
// Also, that file will be located at 0x1e00 by default.  You can make it relocatable by replacing
// the 0x1e in the reset vector and load location with the EELOADER_MSB #define.
BYTE    EELoaderImage[EELOADER_SIZE] = {
0x00, 0x03, 
0x00, 0x00, 
0x02, EELOADER_MSB, 0x00,       // Reset vector.  
0x01, 0x98, 
EELOADER_MSB, 0x00,             // Location of ALL of the code.
0x75, 
0x81, 0x19, 0x02, 0x1F, 0x3B, 0x8A, 0x83, 0x89, 0x82, 0xE4, 
0x73, 0xD1, 0xB4, 0x74, 0xA2, 0xF0, 0x22, 0x8E, 0x14, 0x8F, 
0x15, 0x8D, 0x16, 0x8A, 0x17, 0x8B, 0x18, 0xF1, 0x90, 0xD1, 
0x0C, 0xD1, 0x9F, 0x50, 0x01, 0x22, 0xE5, 0x14, 0xD1, 0x9B, 
0x50, 0x01, 0x22, 0xE5, 0x15, 0xD1, 0x9B, 0x50, 0x01, 0x22, 
0xD1, 0xB4, 0x74, 0xA3, 0xF0, 0xD1, 0x8C, 0x50, 0x01, 0x22, 
0x90, 0xE6, 0x79, 0xE0, 0xF5, 0x19, 0xD1, 0x8C, 0x50, 0x01, 
0x22, 0xE4, 0xF5, 0x19, 0xE5, 0x16, 0x14, 0xFF, 0xE5, 0x19, 
0xC3, 0x9F, 0x50, 0x09, 0xD1, 0x7A, 0x50, 0x01, 0x22, 0x05, 
0x19, 0x80, 0xED, 0x90, 0xE6, 0x78, 0x74, 0x20, 0xF0, 0xD1, 
0x8C, 0x50, 0x01, 0x22, 0xD1, 0x7A, 0x50, 0x01, 0x22, 0x90, 
0xE6, 0x78, 0x74, 0x40, 0xF0, 0xA3, 0xE0, 0xF5, 0x19, 0xC3, 
0x22, 0x90, 0xE6, 0x79, 0xE0, 0xFF, 0xE5, 0x18, 0x25, 0x19, 
0xF5, 0x82, 0xE4, 0x35, 0x17, 0xF5, 0x83, 0xEF, 0xF0, 0x90, 
0xE6, 0x78, 0xE0, 0xFF, 0x30, 0xE0, 0xF8, 0xEF, 0xC3, 0x30, 
0xE2, 0x01, 0xD3, 0x22, 0x90, 0xE6, 0x79, 0xF0, 0x90, 0xE6, 
0x78, 0xE0, 0xFF, 0x30, 0xE0, 0xF8, 0xEF, 0x30, 0xE2, 0x02, 
0xD3, 0x22, 0xEF, 0xC3, 0x20, 0xE1, 0x01, 0xD3, 0x22, 0x90, 
0xE6, 0x78, 0x74, 0x80, 0xF0, 0xA3, 0x22, 0xAF, 0x09, 0xAE, 
0x08, 0x8E, 0x0D, 0x8F, 0x0E, 0x8C, 0x0F, 0x8D, 0x10, 0x8A, 
0x11, 0x8B, 0x12, 0xE5, 0x10, 0x45, 0x0F, 0x60, 0x67, 0xC3, 
0xE5, 0x10, 0x94, 0x10, 0xE5, 0x0F, 0x94, 0x00, 0x7F, 0x10, 
0x50, 0x02, 0xAF, 0x10, 0x8F, 0x13, 0x7E, 0x00, 0xE5, 0x0E, 
0x54, 0x0F, 0xFF, 0x60, 0x19, 0xC3, 0x74, 0x10, 0x9F, 0xFF, 
0xE4, 0x9E, 0xFE, 0xAD, 0x13, 0x7C, 0x00, 0xC3, 0xED, 0x9F, 
0xEC, 0x9E, 0x50, 0x04, 0xAF, 0x05, 0x80, 0x00, 0x8F, 0x13, 
0xAB, 0x12, 0xAA, 0x11, 0xAD, 0x13, 0xAF, 0x0E, 0xAE, 0x0D, 
0xD1, 0x12, 0x50, 0x01, 0x22, 0xAF, 0x13, 0x7E, 0x00, 0xEF, 
0x25, 0x0E, 0xF5, 0x0E, 0xEE, 0x35, 0x0D, 0xF5, 0x0D, 0xEF, 
0x25, 0x12, 0xF5, 0x12, 0xEE, 0x35, 0x11, 0xF5, 0x11, 0xC3, 
0xE5, 0x10, 0x9F, 0xF5, 0x10, 0xE5, 0x0F, 0x9E, 0xF5, 0x0F, 
0x80, 0x93, 0xC3, 0x22, 0x75, 0x08, 0x02, 0x75, 0x09, 0x00, 
0x75, 0x0A, 0x00, 0x75, 0x0B, 0x00, 0x75, 0x0C, 0x00, 0x7A, 
0xE0, 0x7B, 0x00, 0x7D, 0x04, 0x7C, 0x00, 0xD1, 0xBC, 0x74, 
0x04, 0x25, 0x09, 0xF5, 0x09, 0xE4, 0x35, 0x08, 0xF5, 0x08, 
0x90, 0xE0, 0x00, 0xE0, 0x30, 0xE7, 0x06, 0xAA, 0x0B, 0xA9, 
0x0C, 0xD1, 0x06, 0x90, 0xE0, 0x00, 0xE0, 0xFC, 0xA3, 0xE0, 
0xFD, 0xA3, 0xE0, 0xFE, 0xA3, 0xE0, 0xFB, 0xAA, 0x06, 0xD1, 
0xBC, 0x90, 0xE0, 0x00, 0xE0, 0xFE, 0xA3, 0xE0, 0x25, 0x09, 
0xF5, 0x09, 0xEE, 0x35, 0x08, 0xF5, 0x08, 0x80, 0xBA, 0x90, 
0xE6, 0x78, 0xE0, 0x20, 0xE6, 0xF9, 0x22, 0x80, 0x01, 0xE6, 
0x00, 0x00 
};


void Error(char *err)
{
    fprintf(stderr,"Error: ");
    fprintf(stderr,"%s\n",err);
    exit(2);
}

bool get_inputfile(std::map<std::string, docopt::value> args, std::string &file)
{
  bool result = false; 
  auto p = args.find("--source")->second;

  if (p.isString())
  {
     file = p.asString();
     result = true;
  }
  
  return result;
}

bool get_outputfile(std::map<std::string, docopt::value> args, std::string &file)
{
  bool result = false; 
  auto p = args.find("--output")->second;

  if (p.isString())
  {
     file = p.asString();
     result = true;
  }
  
  return result;
}



bool get_memory(std::map<std::string, docopt::value> args, std::string &memory)
{
  bool result = false;
  auto m = args.find("--memory")->second;

  if (m.isString())
  {
     memory   = m.asString();
     result = true;
  }

  return result;
}
#if 0
void ParseCommandLine(int argc, char *argv[])
{
    bool    cont;
    int     i,j,root_length;

    InFilename = NULL;
    for(i=1; i < argc; ++i)
    {
        if((argv[i][0] == '-') || (argv[i][0] == '/'))
        {
            cont = true;
            for(j=1;argv[i][j] && cont;++j)     // Cont flag permits multiple commands in a single argv (like -AR)
                switch(toupper(argv[i][j]))
                {
                    case 'A':
                        OutFileType = FT_ASM;
                        break;
                    case 'E':
                        EELoader = true;
                        OutFileType = FT_IIC;
                        IIC_Reset = true;
                        compressIIC = 1;
                        break;
                    case 'I':
                        OutFileType = FT_IIC;
                        IIC_Reset = true;
                        if (toupper(argv[i][j+1]) == 'C')     // "IC"
                        {
                            compressIIC = 1;
                            j++;    // Consume the 'c' flag
                        }
                        break;
                    case 'B':
                        if (toupper(argv[i][j+1]) == 'I')     // "BI"
                        {
                            InFileType = FT_BIX;
                            j++;        // consume the 'I' flag
                        }
                        else
                            OutFileType = FT_BIX;
                        break;
                    case 'R':
                        IIC_Reset = true;
                        break;
                    case 'M':
                        MemSize = (WORD)strtol(argv[++i],NULL,0);
                        cont = false;
                        break;
                    case 'C':
                        IIC_Hdr.Config0 = (BYTE)strtol(argv[++i],NULL,0);
                        cont = false;
                        break;
                    case 'F':
                        IIC_Hdr.prom_type = (BYTE)strtol(argv[++i],NULL,0);
                        cont = false;
                        break;
                    case 'V':
                        IIC_Hdr.VendorID.w = (WORD)strtol(argv[++i],NULL,0);
                        cont = false;
                        break;
                    case 'P':
                        IIC_Hdr.FnProductID.w = (WORD)strtol(argv[++i],NULL,0);
                        cont = false;
                        break;
                    case 'O':
                        i++;
                        OutFilename = (char *)malloc(strlen(argv[i]+EXT_LENGTH+1));
                        strcpy(OutFilename, argv[i]);
                        cont = false;
                        break;
                    case '?':
                    case 'H':
                        DisplayHelp();
                        break;
                    case 'S':
                        SymbolName = argv[++i];
                        cont = false;
                        break;
                    default:
                        assert(0); //Error(ERR_INVALID_SWTCH);
                }
        }
        else
            InFilename = argv[i];
    }
        
    if(!InFilename && DOWNLOAD_PROM_TYPE)
        DisplayHelp();
    else if (InFilename && !DOWNLOAD_PROM_TYPE)
    {
//        Error(ERR_FTYPE_MISMATCH);
          assert(0);
    } 

    if (OutFilename == NULL)
    {
        root_length = strcspn(InFilename,".");
        OutFilename = (char *)malloc(root_length+EXT_LENGTH);
        memcpy(OutFilename,InFilename,root_length);
        OutFilename[root_length] = 0;
        strcat(OutFilename,Extension[OutFileType]);
    }
    else
    {
//     strcat(OutFilename,Extension[OutFileType]);
    }
}
#endif

bool GetNextBlock(DWORD *addr)
{
    while(!ImageMap[*addr] && (*addr < MemSize))
        ++*addr;
    if(*addr == MemSize)
        return(false);
    return(true);
}

// Gets the max block size we want to write to the .iic file.
// If the .iic file compression switch is set, we only stop the block for 
// three blanks in a row (the header is four bytes)
DWORD GetBlockLen(DWORD addr)
{
    int i = 0;
    int start = addr;

    if (compressIIC)
        while((ImageMap[addr] || ImageMap[addr+1] || ImageMap[addr+2]) && (addr < MemSize) && (i++ < 0x3ff))
            ++addr;
    else
        while(ImageMap[addr] && (addr < MemSize) && (i++ < 0x3ff))
            ++addr;

    return(addr - start);
}

#if 0
void DisplayHeader(void)
{


void DisplayHelp(void)
{
    fprintf(stdout,"\nHEX2BIX [-AEIBRH?] [-IC] [-S symbol] [-M memsize] [-C Config0Byte] [-F firstByte] [-O filename] [-X] Source\n\n");
    fprintf(stdout,"   Source - Input filename\n");
    fprintf(stdout,"   A      - Output file in the A51 file format\n");
    fprintf(stdout,"   B      - Output file in the BIX (raw binary) format (Default)\n");
    fprintf(stdout,"   BI     - Input file in the BIX (raw binary) format (hex is default)\n");
    fprintf(stdout,"   C      - Config0 BYTE for AN2200 and FX2 (Default = 0x04)\n");
    fprintf(stdout,"   E      - Create .IIC file for External RAM. Prepends 0x200 byte loader that\n");
    fprintf(stdout,"            loads at 0x%x00\n", EELOADER_MSB);
    fprintf(stdout,"   F      - First byte (0xB0, 0xB2, 0xB6, 0xC0, 0xC2) (Default = 0xB2)\n");
    fprintf(stdout,"   H|?    - Display this help screen\n");
    fprintf(stdout,"   I      - Output file in the IIC file format (sets -R)\n");
    fprintf(stdout,"   IC     - Output Compressed IIC file (sets -R)\n");
    fprintf(stdout,"   M      - Max Memory(RAM) limit for firmware. (Default=8k)\n");
    fprintf(stdout,"   O      - Output filename\n");
    fprintf(stdout,"   P      - Product ID (Default = 2131)\n");
    fprintf(stdout,"   R      - Append bootload block to release reset\n");
    fprintf(stdout,"   S      - Public symbol name for linking\n");
    fprintf(stdout,"   V      - Vendor ID (Default = 0x0547) \n");
    exit(0);
}
#endif


bool get_sourcefile(std::map<std::string, docopt::value> args, std::string &file)
{
  bool result = false; 
  auto p = args.find("--source")->second;

  if (p.isString())
  {
     file = p.asString();
     result = true;
  }
  
  return result;
}

int main(int argc, char *argv[])
{
    BYTE        rec_length, rec_type;
    unsigned char  rec_chksum, chksum;
    DWORD       rec_addr,addr,len, bytes = 0;
    BLOCK_HDR   block_hdr;
    char        line[LINE_LENGTH + 1],tmp[5],time_str[TIME_STR_LEN],date_str[DATE_STR_LEN];
    DWORD       i,j;
    FILE        *file;
    time_t      time_now;
    DWORD totalCodeBytes = 0;
    bool  failed_init = false;

	static const char USAGE[] =
	R"(hex2bix.
           Intel Hex file to EZ-USB Binary file conversion utility
           Copyright (c) 2012-2013, Cypress Semiconductor Inc.

	    Usage:
	      hex2bix  --source=<hexfilename> --output=<outputfilename> [--maxmemory=<maxmemory>] [--configbyte=<configbyte>] [--firstbyte=<firstbyte>] [--bix] [--a51] [--appendreset] [--iic] [--iicc]
	      hex2bix  --source=<hexfilename> --output=<outputfilename> [--maxmemory=<maxmemory>] [--configbyte=<configbyte>] [--firstbyte=<firstbyte>] [--bix] [--a51] [--appendreset] [--iic] [--iicc]
              hex2bix --a51
	      hex2bix (-h | --help)
	      hex2bix --version
	    Options:
	      -h --help                    Show this screen.
	      --version                    Show version.
	      --iic                        Output file in IIC file for External RAM.
              --iicc                       Output file in compressed IC file format. Sets --appendreset.
	      --a51                        Output file in A51 file format.
              --bix                        Output file in BIX (raw binary) format.
              --appendreset                Append bootload block to release reset.
	      --source=<hexfilename>       Hex Filename
              --output=<ouputfilename>     Output Filename
              --maxmemory=<maxmemory>      Set maximum memory (RAM) limit for firmware.
              --configbyte=<configbyte>    Set config0 byte for AN2200 and FX2. Default ix 0x04.
              --firstbyte=<firstbyte>      Set first byte (0xB0, 0xB2, 0xB6, 0xC0, 0xC2). Default is 0xB2.
	)";

   std::map<std::string, docopt::value> args
        = docopt::docopt(USAGE,
                         { argv + 1, argv + argc },
                         true,                // show help if requested
                         "hex2bix 2.0");  // version string

bool a51_found            = false;
bool iic_found            = false;
bool iic_compressed_found = false;
bool bix_found            = false;
bool maxmemory_found      = false;
bool configbyte_found     = false;
bool firstbyte_found      = false;
bool appendreset_found    = false;

  auto arg = args.find("--iic");
  if(arg != args.end())
  {
      if (arg->second.isBool())
      {
         iic_found = arg->second.asBool();
         OutFileType = FT_IIC;
         IIC_Reset = true;
      }
  }

  arg = args.find("--iicc");
  if(arg != args.end())
  {
      if (arg->second.isBool())
      {
         iic_compressed_found = arg->second.asBool();
         compressIIC = 1;
         OutFileType = FT_IIC;
         IIC_Reset = true;
      }
  }

  arg = args.find("--bix");
  if(arg != args.end())
  {
      if (arg->second.isBool())
      {
         bix_found = arg->second.asBool();
      }
  }

  arg = args.find("--a51");
  if(arg != args.end())
  {
      if (arg->second.isBool())
      {
         OutFileType = FT_ASM;
         a51_found = arg->second.asBool();
      }
  }

  if (!get_inputfile(args, InFilename))
  {
     std::cout << "sourcefile: invalid file defined" << std::endl;
     failed_init = true;
  }
  else if (!get_outputfile(args, OutFilename))
  {
     std::cout << "outputfile: invalid file defined" << std::endl;
     failed_init = true;
  }

  if (failed_init)
  {
     return 1;
  }

  


    Image = (BYTE *)malloc(MEMORY_BUFFER_SIZE);
    ImageMap = (BYTE *)malloc(MEMORY_BUFFER_SIZE+2);        // add two extra bytes to the image to help with iic compression.
    memset(Image,MEM_FILL,MEMORY_BUFFER_SIZE);
    memset(ImageMap,0,MEMORY_BUFFER_SIZE);

    if (true) //(InFilename.)
        if (InFileType == FT_HEX)
        {
            file = fopen(InFilename.c_str(),"r");

            if(!file)
            {
            //    Error(ERR_INFILE);
                  assert(0);
            }

            while(fgets(line,LINE_LENGTH,file))
            {
                if(line[0] != ':')
                {
                   // Error(ERR_INVALID_FILE);
                    assert(0);
                } 
                memcpy(tmp,&line[1],2);
                tmp[2] = 0;
                rec_length = (BYTE)strtol(tmp,NULL,16);
                memcpy(tmp,&line[3],4);
                tmp[4] = 0;
                rec_addr = (WORD)strtol(tmp,NULL,16);
                memcpy(tmp,&line[7],2);
                tmp[2] = 0;
                rec_type = (BYTE)strtol(tmp,NULL,16);

                if(rec_type == 1)
                    break;

                chksum = 0;

                if (rec_addr + rec_length > MemSize)
                {
                    fprintf(stderr, "Error:  Input image too big for output file size.  Addr = 0x%x, len = 0x%x.\n", rec_addr, rec_length);
                    exit(2);
                }

                for(i=0;(i < rec_length) && (i < MemSize - rec_addr); ++i)
                {
                    memcpy(tmp,&line[(i*2)+9],2);
                    tmp[2] = 0;
                    chksum += (Image[rec_addr+i] = (BYTE)strtol(tmp,NULL,16));
                    ImageMap[rec_addr+i] = true;
                }

                chksum = (~chksum) + 1;
                memcpy(tmp,&line[(i*2)+9],2);
                tmp[2] = 0;
                rec_chksum = (BYTE)strtol(tmp,NULL,16);
#if 0
                if(rec_chksum != chksum)
                {
                    //Error(ERR_BAD_CHKSUM);
                    std::cout << "Error: Bad Checksum, Expected: " << std::dec << (uint8_t)rec_chksum << " Actual: " << chksum << std::endl;
                    return 1;
                }
#endif

            }

            fclose(file);
        }
        else
        {
            int numread;

            file = fopen(InFilename.c_str(),"r");

            if(!file)
            {
                //Error(ERR_INFILE);
                assert(0);
            }
               
            numread = fread( Image, sizeof( char ), MemSize, file );
            memset(ImageMap,true,numread);
        }

    file = fopen(OutFilename.c_str(),"wb");

    switch(OutFileType)
    {
        case FT_BIX:
            if(!file)
            {
                //Error(ERR_OUTFILE);
                assert(0);
            }
            
            fwrite(Image,sizeof(BYTE),MemSize,file);
            bytes = MemSize;
            break;
        case FT_IIC:
        {
            switch (IIC_Hdr.prom_type)
            {
                case 0xB0:                       // EZ-USB
                case 0xB2:
                    fwrite(&IIC_Hdr,7,1,file);    // Write type, VID, PID, DID (7 bytes)
                    bytes = 7;
                    break;
                case 0xB4:                       // EZ-USB FX
                case 0xB6:
                    fwrite(&IIC_Hdr,9,1,file);    // Write type, VID, PID, DID, Config (9 bytes)
                    bytes = 9;
                    break;
                case 0xC0:                       // EZ-USB FX2
                case 0xC2:
                    fwrite(&IIC_Hdr,8,1,file);    // Write type, VID, PID, DID, Config (8 bytes)
                    bytes = 8;
                    break;
                default:
                    {
                        //Error(ERR_UNRECOGNIZED_FIRSTBYTE);
                        assert(0);
                    }
            }
        }

            if (EELoader)
            {
                for (i = EELOADER_MSB << 8; i < (EELOADER_MSB << 8) + EELOADER_SIZE; i++)
                {
                    if (ImageMap[i])
                    {
                        fprintf(stderr, "Error:  Code image overlaps EEPROM loader at address 0x%x\n", i);
                        exit(2);
                    }
                }
                fwrite(EELoaderImage, EELOADER_SIZE-bytes, 1, file);
                bytes = EELOADER_SIZE;
            }
            addr = 0;

            while(GetNextBlock(&addr))
            {
                len = GetBlockLen(addr);
                totalCodeBytes += len;
                block_hdr.length.w = (WORD)len;
                block_hdr.start.w = (WORD)addr;
                LE(block_hdr.length);
                LE(block_hdr.start);
                fwrite(&block_hdr,sizeof(BLOCK_HDR),1,file);
                bytes += sizeof(BLOCK_HDR);
                for(i=0;i<len;++i)
                {
                    fwrite(&Image[addr+i],sizeof(BYTE),1,file);
                    ++bytes;
                }
                addr += len;
            }

            if (bytes && !DOWNLOAD_PROM_TYPE)
            {
                printf("%x\n", addr);
                assert(0);
                //Error(ERR_FTYPE_MISMATCH);
            }

            if(bytes && IIC_Reset)
            {
                // Reset register is in a different location in FX2
                if (IIC_Hdr.prom_type == 0xC2)
                   fwrite(Fx2ResetString,sizeof(BYTE),RST_STR_LEN,file);
                else
                   fwrite(ResetString,sizeof(BYTE),RST_STR_LEN,file);
                bytes += RST_STR_LEN;
            }
            break;
        case FT_ASM:
            time(&time_now);
            snprintf(time_str,TIME_STR_LEN,"%8s",asctime(localtime(&time_now)));
            snprintf(date_str+19,DATE_STR_LEN,"%s",asctime(localtime(&time_now)));

            fprintf(file,";;--------------------------------------------------------------------------------------\r\n");
            fprintf(file,";; File:\t%s\r\n",OutFilename.c_str());
            fprintf(file,";; Source:\t%s\r\n",InFilename.c_str());
            fprintf(file,";; Date:\t%s at %s\r\n",date_str,time_str);
            fprintf(file,";;--------------------------------------------------------------------------------------\r\n");
            fprintf(file,"%s_seg\tsegment\tcode\r\n", SymbolName);
            fprintf(file,"\t\trseg\t%s_seg\r\n", SymbolName);
            fprintf(file,"public\t\t%s\r\n", SymbolName);
            fprintf(file,"%s:\r\n",SymbolName);

            addr = 0;

            while(GetNextBlock(&addr))
            {
                len = GetBlockLen(addr);
                fprintf(file,"\t\t; Type = %02xH, Start = %04xH, Length = %04xH\r\n",(BYTE)BLCK_CONTINUE, addr,len);
                fprintf(file,"\t\tdb\t%03xH,%03xH,%03xH,%03xH,%03xH\r\n",(BYTE)BLCK_CONTINUE, MSB(addr),LSB(addr),MSB(len),LSB(len));
                bytes += 5;
                for(i=0;i<len;)
                {
                    fprintf(file,"\t\tdb\t%03xH",Image[addr+i]);
                    ++i;
                    for(j=0;(j < 15) && ((j+i) < len);++j)
                        fprintf(file,",%03xH",Image[addr+j+i]);
                    fprintf(file,"\r\n");
                    i += j;
                }
                bytes += len;
                fprintf(file,"\r\n");
                addr += len;
            }
            fprintf(file,"\t\tdb\t%03xH\t\t;End of blocks\r\n",BLCK_END);
            ++bytes;
            fprintf(file,"\t\tend\r\n");
            break;

    }
    fclose(file);

    if (bytes)
    {
        fprintf(stdout,"%d Bytes written.\n",bytes);
        fprintf(stdout,"Total Code Bytes = %d\n",totalCodeBytes);
    }

    fprintf(stdout,"Conversion completed successfully.\n");
    return(0);
}
