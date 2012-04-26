#ifndef _FLOPPY_H
#define	_FLOPPY_H

#include <stdint.h>
#include <vector>

#define TRACKLEN 6250

#define DISK_TYPE_TRD	1

#define	FLP_TRK		0
#define	FLP_RTRK	1
#define	FLP_FIELD	2
#define	FLP_POS		3
#define	FLP_DISKTYPE	10

#define	ERR_OK		0
#define	ERR_MANYFILES	1
#define	ERR_NOSPACE	2
#define	ERR_SHIT	3

#define	FLP_INSERT	1
#define	FLP_PROTECT	(1<<1)
#define	FLP_TRK80	(1<<2)
#define	FLP_DS		(1<<3)
#define	FLP_INDEX	(1<<4)
#define	FLP_MOTOR	(1<<5)
#define	FLP_HEAD	(1<<6)
#define	FLP_CHANGED	(1<<7)
#define	FLP_SIDE	(1<<8)

typedef struct {
	uint8_t cyl;
	uint8_t side;
	uint8_t sec;
	uint8_t len;
	uint8_t* data;
	uint8_t type;
	int32_t crc;
} Sector;

typedef struct {
	uint8_t name[8];
	uint8_t ext;
	uint8_t lst,hst;
	uint8_t llen,hlen;
	uint8_t slen;
	uint8_t sec;
	uint8_t trk;
} TRFile;

typedef struct {
	int flag;
	uint8_t id;
	uint8_t iback;
	uint8_t trk,rtrk;
	uint8_t field;
	int32_t pos;
	uint32_t ti;
	std::string path;
	struct {
		uint8_t byte[TRACKLEN];
		uint8_t field[TRACKLEN];
	} data[256];
} Floppy;

Floppy* flpCreate(int);
void flpDestroy(Floppy*);

uint8_t flpRd(Floppy*);
void flpWr(Floppy*,uint8_t);
bool flpEject(Floppy*);
uint8_t flpGetField(Floppy*);
bool flpNext(Floppy*,bool);		// return true on index strobe
void flpStep(Floppy*,bool);

int flpGet(Floppy*,int);
void flpSet(Floppy*,int,int);

void flpFormat(Floppy*);
void flpFormTrack(Floppy*,int,std::vector<Sector>);
void flpFormTRDTrack(Floppy*,int,uint8_t*);
void flpClearTrack(Floppy*,int);
void flpFillFields(Floppy*,int,bool);

bool flpGetSectorData(Floppy*,uint8_t,uint8_t,uint8_t*,int);
bool flpGetSectorsData(Floppy*,uint8_t,uint8_t,uint8_t*,int);
bool flpPutSectorData(Floppy*,uint8_t,uint8_t,uint8_t*,int);
void flpPutTrack(Floppy*,int,uint8_t*,int);
void flpGetTrack(Floppy*,int,uint8_t*);
void flpGetTrackFields(Floppy*,int,uint8_t*);

int flpCreateFile(Floppy*,TRFile*);
std::vector<TRFile> flpGetTRCatalog(Floppy*);
TRFile flpGetCatalogEntry(Floppy*, int);

#endif