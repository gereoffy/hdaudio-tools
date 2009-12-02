// ParseD2V by Donald A. Graft
// Released under the Gnu Public Licence (GPL)

//#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
//#include "global.h"

#define D2V_FILE_VERSION 16

#define bool char
#define true 1
#define false 0


// Return 1 if a transition was detected in test_only mode and the user wants to correct it, otherwise 0.	
int fix_d2v(char *Input, int test_only){
	FILE *fp, *wfp, *dfp;
	char line[2048], prev_line[2048], wfile[2048], logfile[2048], *p, *q;
	int val, mval, prev_val, mprev_val, fix;
	bool first, found;
	int D2Vformat = 0;
	unsigned int info;

	fp = fopen(Input, "r");
	if (fp == 0)
	{
//		MessageBox(hWnd, "Cannot open the D2V file!", NULL, MB_OK | MB_ICONERROR);
		return 0;
	}
	// Pick up the D2V format number
	fgets(line, 1024, fp);
	if (strncmp(line, "DGIndexProjectFile", 18) != 0)
	{
//		MessageBox(hWnd, "The file is not a DGIndex project file!", NULL, MB_OK | MB_ICONERROR);
		fclose(fp);
		return 0;
	}
	sscanf(line, "DGIndexProjectFile%d", &D2Vformat);
	if (D2Vformat != D2V_FILE_VERSION)
	{
//		MessageBox(hWnd, "Obsolete D2V file.\nRecreate it with this version of DGIndex.", NULL, MB_OK | MB_ICONERROR);
		fclose(fp);
		return 0;
	}

	if (!test_only)
	{
		strcpy(wfile, Input);
		strcat(wfile,".fixed");
		wfp = fopen(wfile, "w");
		if (wfp == 0)
		{
//			MessageBox(hWnd, "Cannot create the fixed D2V file!", NULL, MB_OK | MB_ICONERROR);
			fclose(fp);
			return 0;
		}
		fputs(line, wfp);
		// Mutate the file name to the output text file to receive processing status information.
		strcpy(logfile, Input);
		p = &logfile[strlen(logfile)];
		while (*p != '.') p--;
		p[1] = 0;
		strcat(p, "fix.txt");
		// Open the output file.
		dfp = fopen(logfile, "w");
		if (dfp == 0)
		{
			fclose(fp);
			fclose(wfp);
//			MessageBox(hWnd, "Cannot create the info output file!", NULL, MB_OK | MB_ICONERROR);
			return 0;
		}

		fprintf(dfp, "D2V Fix Output\n\n");
	}
	
	while (fgets(line, 1024, fp) != 0)
	{
		if (!test_only) fputs(line, wfp);
		if (strncmp(line, "Location", 8) == 0) break;
	}
	fgets(line, 1024, fp);
	if (!test_only) fputs(line, wfp);
	fgets(line, 1024, fp);
	prev_line[0] = 0;
	prev_val = -1;
	mprev_val = -1;
	found = false;
	do
	{
		p = line;
    		sscanf(p, "%x", &info);
    		// If it's a progressive sequence then we can't have any transitions.
    		if (info & (1 << 9)) continue;

		while (*p++ != ' ');
		while (*p++ != ' ');
		while (*p++ != ' ');
		while (*p++ != ' ');
		while (*p++ != ' ');
		while (*p++ != ' ');
		while (*p++ != ' ');
		first = true;
		while ((*p >= '0' && *p <= '9') || (*p >= 'a' && *p <= 'f'))
		{
			fix = -1;
			sscanf(p, "%x", &val);
			if (val == 0xff) break;
			// Isolate the TFF/RFF bits.
			mval = val & 0x3;
			if (prev_val >= 0) mprev_val = prev_val & 0x3;
			// Detect field order transitions.
			if (mval == 2 && mprev_val == 0)      fix = 1;
			else if (mval == 3 && mprev_val == 0) fix = 1;
			else if (mval == 0 && mprev_val == 1) fix = 0;
			else if (mval == 1 && mprev_val == 1) fix = 0;
			else if (mval == 0 && mprev_val == 2) fix = 3;
			else if (mval == 1 && mprev_val == 2) fix = 3;
			else if (mval == 2 && mprev_val == 3) fix = 2;
			else if (mval == 3 && mprev_val == 3) fix = 2;
			// Correct the field order transition.
			if (fix >= 0)
			{
				found = true;
				if (!test_only) fprintf(dfp, "Field order transition: %x -> %x\n", mprev_val, mval);
				if (!test_only) fprintf(dfp, prev_line);
				if (!test_only) fprintf(dfp, line);
				if (first == false)
				{
					q = p;
					while (*q-- != ' ');
				}
				else
				{
					q = prev_line;
					while (*q != '\n') q++;
					while (!((*q >= '0' && *q <= '9') || (*q >= 'a' && *q <= 'f'))) q--;
				}
				*q = (char) fix + '0';
				if (!test_only) fprintf(dfp, "corrected...\n");
				if (!test_only) fprintf(dfp, prev_line);
				if (!test_only) fprintf(dfp, line);
				if (!test_only) fprintf(dfp, "\n");
			}
			while (*p != ' ' && *p != '\n') p++;
			p++;
			prev_val = val;
			first = false;
		}
		if (!test_only) fputs(prev_line, wfp);
		strcpy(prev_line, line);
	} while ((fgets(line, 2048, fp) != 0) &&
			 ((line[0] >= '0' && line[0] <= '9') || (line[0] >= 'a' && line[0] <= 'f')));
	if (!test_only) fputs(prev_line, wfp);
	if (!test_only) fputs(line, wfp);
	while (fgets(line, 2048, fp) != 0)
		if (!test_only) fputs(line, wfp);
	fclose(fp);
	if (!test_only) fclose(wfp);
	if (test_only)
	{
    		if (found == true) return 1;
		return 0;
	}
	if (found == false)
	{
		fprintf(dfp, "No errors found.\n");
		fclose(dfp);
		unlink(wfile);
//		MessageBox(hWnd, "No errors found.", "Fix D2V", MB_OK | MB_ICONINFORMATION);
//		return 0;
	}

	return 0;
}



int main(int argc,char* argv[]){
    char nev[1000];
    strcpy(nev,argv[1]);
    return fix_d2v(nev, 0);
}

