/* :ts=2 */
#include <devices/scsidisk.h>

#include <exec/exec.h>

#include <proto/dos.h>
#include <proto/exec.h>

#include <stdio.h>
#include <string.h>

#ifdef __VBCC__
#include <stdlib.h>
#define __aligned
static void cleanup(void);
#endif

#include "toolbox_version.h"

static UBYTE versiontag[] = VERSTAG;

static struct RDArgs *args;

enum {
	ARG_DEVICE,
	ARG_UNIT,
	ARG_LD,
	ARG_LF,
	ARG_LCD,
	ARG_SCD,
	ARG_GET,
	ARG_INQ,
	ARG_TR,
	ARG_NARG,
};
static LONG argsarray[ARG_NARG];

static const char *device = "scsi.device";
static LONG unit = 2;

static struct MsgPort *msgport;
static struct IOStdReq *ior;

__aligned union {
	struct toolbox_file {
		UBYTE index;
		UBYTE isdir;
		char  name[33];
		ULONG size;
	} files[100];
	UBYTE data[4096];
} data;

static UBYTE command[10];

static BPTR file;

static const char * const devicetypes[] = {
	"Fixed",
	"Removable",
	"Optical",
	"Floppy",
	"Magneto-Optical",
	"Sequential",
	"Network",
	"ZIP100",
};

static const char * const periph_devtypes[] = {
	"Direct access",
	"Sequential access",
	"Printer",
	"Processor",
	"Write-once",
	"CD-ROM",
	"Scanner",
	"Optical memory",
	"Medium changer",
	"Communications",
};

static const char * const ansi_versions[] = {
	"Not claimed",
	"SCSI-1",
	"SCSI-2",
	"SPC",
	"SPC-2",
	"SPC-3",
	"SPC-4",
	"SPC-5",
};


int main(void)
{
	struct SCSICmd scsicmd;
	int nactions;

#ifdef __VBCC__
	atexit(cleanup);
#endif

	if (DOSBase->dl_lib.lib_Version < 36) {
		Printf("dos.library v36 or later is required\n");
		return RETURN_ERROR;
	}

	argsarray[ARG_DEVICE] = (LONG)device;
	argsarray[ARG_UNIT] = (LONG)&unit;

	args = ReadArgs("DEVICE,UNIT/N,LD=LISTDEVICES/S,LF=LISTFILES/S,"
			"LCD=LISTCDS/S,SCD=SETCD/N,GET,"
			"INQ=INQUIRY/S,TR=TESTREADY/S",
			argsarray, NULL);
	if (args == NULL) {
		PrintFault(IoErr(), "Unable to read arguments");
		return RETURN_ERROR;
	}

	nactions = (argsarray[ARG_LD] != 0) +
		   (argsarray[ARG_LF] != 0) +
		   (argsarray[ARG_LCD] != 0) +
		   (argsarray[ARG_SCD] != 0) +
		   (argsarray[ARG_GET] != 0) +
		   (argsarray[ARG_INQ] != 0) +
		   (argsarray[ARG_TR] != 0);
	if (nactions == 0) {
		Printf("Must specify an action\n");
		return RETURN_ERROR;
	}
	if (nactions != 1) {
		Printf("Must specify at most one action\n");
		return RETURN_ERROR;
	}

	if (!*(const char *)argsarray[ARG_DEVICE]) {
		Printf("DEVICE must not be empty\n");
		return RETURN_ERROR;
	}

	if (argsarray[ARG_GET] &&
	    !*(const char *)argsarray[ARG_GET]) {
		Printf("GET must not be empty\n");
		return RETURN_ERROR;
	}

	if (argsarray[ARG_SCD] &&
			!(UBYTE)*(LONG *)argsarray[ARG_SCD]) {
		Printf("SETCD index must be greater than 0\n");
		return RETURN_ERROR;
	}

	device = (const char *)argsarray[ARG_DEVICE];
	unit = *(LONG *)argsarray[ARG_UNIT];

	msgport = CreateMsgPort();
	if (msgport == NULL) {
		Printf("Unable to create message port\n");
		return RETURN_ERROR;
	}
	ior = (struct IOStdReq *)CreateIORequest(msgport, sizeof(*ior));
	if (ior == NULL) {
		Printf("Unable to create IO request\n");
		return RETURN_ERROR;
	}

	if(OpenDevice(device, unit, (struct IORequest *)ior, 0L)) {
		Printf("Unable to open device %s unit %ld\n", device, unit);
		return RETURN_ERROR;
	}

	ior->io_Command = HD_SCSICMD;
	ior->io_Data = &scsicmd;
	ior->io_Length = sizeof(scsicmd);
	if (argsarray[ARG_LD]) {
		command[0] = 0xD9;
	} else if (argsarray[ARG_LF]) {
		command[0] = 0xD0;
	} else if (argsarray[ARG_LCD]) {
		command[0] = 0xD7;
	} else if (argsarray[ARG_SCD]) {
		command[0] = 0xD8;
		command[1] = (UBYTE)*(LONG *)argsarray[ARG_SCD] - 1;
	} else if (argsarray[ARG_GET]) {
		command[0] = 0xD0;
	} else if (argsarray[ARG_INQ]) {
		command[0] = 0x12;
		command[4] = 36;
	} else if (argsarray[ARG_TR]) {
		command[0] = 0x00;
	}
	scsicmd.scsi_Command = command;
	scsicmd.scsi_CmdLength =
		(argsarray[ARG_INQ] || argsarray[ARG_TR]) ? 6 : sizeof(command);
	scsicmd.scsi_Data = (UWORD *)&data;
	scsicmd.scsi_Length = argsarray[ARG_TR] ? 0 : sizeof(data);
	scsicmd.scsi_Flags = SCSIF_READ;

	if (argsarray[ARG_TR]) {
		if (DoIO((struct IORequest *)ior))
			Printf("Device %s unit %ld is not ready\n",
			       device, unit);
		else
			Printf("Device %s unit %ld is ready\n",
			       device, unit);
		return RETURN_OK;
	}

	if (DoIO((struct IORequest *)ior)) {
		Printf("Unable to send IO request: %ld\n", ior->io_Error);
		return RETURN_ERROR;
	}

	if (argsarray[ARG_LF]) {
		int i, nfiles;
		Printf("%-32s %-10s\n", "Name", "Size");
		Printf("-------------------------------------------\n");
		nfiles = scsicmd.scsi_Actual / sizeof(struct toolbox_file);
		for (i = 0; i < nfiles; i++) {
			struct toolbox_file *f = &data.files[i];
			Printf("%-32s %-10ld\n", f->name, f->size);
		}
	} else if (argsarray[ARG_LCD]) {
		int i, nfiles;
		Printf("%-6s %-32s %-10s\n", "Index", "Name", "Size");
		Printf("--------------------------------------------------\n");
		nfiles = scsicmd.scsi_Actual / sizeof(struct toolbox_file);
		for (i = 0; i < nfiles; i++) {
			struct toolbox_file *f = &data.files[i];
			Printf("%-6ld %-32s %-10ld\n",
			       f->index + 1, f->name, f->size);
		}
	} else if (argsarray[ARG_LD]) {
		int i;
		Printf("%-3s %-32s\n", "ID", "Type");
		Printf("------------------------------------\n");
		for (i = 0; i < scsicmd.scsi_Actual; i++) {
			UBYTE t = data.data[i];
			const char *s;
			if (t < sizeof(devicetypes)/sizeof(devicetypes[0])) {
				s = devicetypes[t];
			} else if (t == 255) {
				s = "Not enabled";
			} else {
				s = "Unknown";
			}
			Printf("%-3ld %-32s\n", i, s);
		}
	} else if (argsarray[ARG_INQ]) {
		char vendor[9], product[17], revision[5];
		UBYTE devtype = data.data[0] & 0x1F;
		UBYTE rmb = (data.data[1] >> 7) & 1;
		UBYTE ansi = data.data[2] & 0x07;
		const char *dtstr, *ansistr;

		memcpy(vendor, &data.data[8], 8);
		vendor[8] = '\0';
		memcpy(product, &data.data[16], 16);
		product[16] = '\0';
		memcpy(revision, &data.data[32], 4);
		revision[4] = '\0';

		if (devtype < sizeof(periph_devtypes)/sizeof(periph_devtypes[0]))
			dtstr = periph_devtypes[devtype];
		else
			dtstr = "Unknown";

		if (ansi < sizeof(ansi_versions)/sizeof(ansi_versions[0]))
			ansistr = ansi_versions[ansi];
		else
			ansistr = "Unknown";

		Printf("Vendor:   %s\n", vendor);
		Printf("Product:  %s\n", product);
		Printf("Revision: %s\n", revision);
		Printf("Type:     %s%s\n", dtstr, rmb ? " (removable)" : "");
		Printf("ANSI:     %s\n", ansistr);
	} else if (argsarray[ARG_GET]) {
		ULONG i, nfiles;
		ULONG nblocks;
		const char *fpart;
		nfiles = scsicmd.scsi_Actual / sizeof(struct toolbox_file);
		fpart = FilePart((const char *)argsarray[ARG_GET]);
		for (i = 0; i < nfiles; i++) {
			struct toolbox_file *f = &data.files[i];
			if (!strcmp(fpart, f->name)) {
				break;
			}
		}
		if (i == nfiles) {
			Printf("File \"%s\" not found\n", fpart);
			return RETURN_ERROR;
		}

		file = Open((const char *)argsarray[ARG_GET], MODE_NEWFILE);
		if (!file) {
			PrintFault(IoErr(),
				   "Unable to open file for writing");
			return RETURN_ERROR;
		}

		nblocks = (data.files[i].size / 4096) +
			  (data.files[i].size % 4096 ? 1 : 0);

		command[0] = 0xD1;
		command[1] = i;

		for (i = 0; i < nblocks; i++) {
			ULONG actual;
			memcpy(&command[2], &i, 4);
			if (DoIO((struct IORequest *)ior)) {
				Printf("Unable to send IO request: %ld\n",
				       ior->io_Error);
				return RETURN_ERROR;
			}

			Printf("%s%s: Block %ld/%ld", i ? "\xd" : "",
			       (const char *)argsarray[ARG_GET],
			       i + 1, nblocks);

			actual = scsicmd.scsi_Actual;
			if (Write(file, data.data, actual) != actual) {
				PrintFault(IoErr(),
					   "\nUnable to write to file");
				Close(file);
				file = 0;
				DeleteFile((const char *)argsarray[ARG_GET]);
				return RETURN_ERROR;
			}
		}
		Printf("\n");
	}

	return RETURN_OK;
}

#ifdef __VBCC__
static void cleanup(void)
#else
void _STD_cleanup(void)
#endif
{
	if (file)
		Close(file);

	if (ior) {
		if (ior->io_Device)
			CloseDevice((struct IORequest *)ior);
		DeleteIORequest(ior);
	}

	if (msgport)
		DeleteMsgPort(msgport);

	if (args)
		FreeArgs(args);
}
