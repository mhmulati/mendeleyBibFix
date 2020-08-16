/*
 * mendeleyBibFix - correct formatting of bib-files that are automatically
 * 	generated by Mendeley Desktop
 *
 * NOTE: Mendeley Desktop is copyright 2008-2019 by Mendeley Ltd.
 * This software is not provided by Mendeley and the author has no affiliation
 * with their company.
 *
 * Documentation:
 * This is a simple function intended to correct bib-files that are
 * automatically generated by Mendeley Desktop. I have found it to work
 * for bib-files generated with the IEEE citation style, but it should
 * work for other styles as well. It makes the following corrections:
 * 		- changes double braces around titles to single braces
 * 		- removes escaping of { and } (will only matter if you checked
 * 			"Escape LaTeX special characters" in the "Bibtex" Options tab)
 * 		- removes URL for any entry that is not specified as an exception
 *			(read the comment block after start of main function to read
 *			how to change the exceptions)
 * 		- removes braces around months
 *
 * It should work correctly for files generated by Mendeley Desktop v1.16.1.
 * Still functioning as of v1.19.5.
 *
 * A number of fixes are hard-coded, i.e., it expects to know where the braces are.
 * So this code runs very fast (bib files with hundreds of entries are fixed in a
 * small fraction of a second) but may not be "future-proof"
 *
 * You will need to compile this code to run it. A compiled version for Windows is
 * included on the release page of Github. If you are going to compile it yourself with gcc,
 * then you will need the -std=c99 option
 *
 * Call syntax (Windows):
 * 		mendeleyBibFix.exe [OUTPUT_FILENAME] [INPUT_FILENAME]
 * Call syntax (Linux or macOS):
 * 		./mendeleyBibFix [OUTPUT_FILENAME] [INPUT_FILENAME]
 *
 * Both arguments are optional. If there is only one argument, then it is assumed to be
 * the output filename. The default input filename is "library.bib", and the default
 * output filename is "library_fixed.bib". If you're fine with the defaults, then
 * you can also just double-click on the executable without needing a terminal open.
 *
 * Copyright 2016-2019 Adam Noel. All rights reserved.
 * Distributed under the New BSD license. See LICENSE.txt for license details.
 *
 * Created June 15, 2016
 * Current version v1.2.2 (2019-08-26)
 *
 * Revision history:
 *
 * Revision v1.2.2 (2019-08-26)
 * - modified detection of fields to search for a newline character after the "},". This
 * helps to prevent the partial removal of the "annote" field when the text within
 * includes curly braces.
 *
 * Revision v1.2.1 (2017-04-26)
 * - fixed removal of "file" field to properly deal with accented names in the file name
 *
 * Revision v1.2 (2017-03-17)
 * - added removal of "file" field, which lists location of local soft copy
 *
 * Revision v1.1 (2016-10-26)
 * - added removal of "annote" field, which includes personal annotations
 *
 * Revision v1.0.3 (2016-06-19)
 * - corrected detection of bib entry after a URL that gets removed
 * - added workaround to enable a custom date, "to appear", "in press", or any other custom
 * 		data at end of entry. If an entry has an ISSN but no year, then the ISSN is renamed
 *		to the year.
 *
 * Revision v1.0.2 (2016-06-15)
 * - removed unused variables
 *
 * Revision v1.0.1 (2016-06-15)
 * - corrected end of bib entry detection to not catch annotations as false alarms
 *
 * Revision v1.0 (2016-06-15)
 * - File created
 *
 *
*/

#include <stdio.h>
#include <stdlib.h>   // for exit(), malloc
#include <string.h>   // for strcpy()
#include <stdbool.h>  // for C++ bool naming, requires C99
#include <time.h>     // For time record keeping

#define BIB_TYPE_MAX 25

// Function declarations
char * stringAllocate(long stringLength);
char * stringWrite(char * src);
unsigned long findEndOfLine(char * str, unsigned long startInd);
unsigned long findEndOfField(char * str, unsigned long startInd);

//
// MAIN
//
int main(int argc, char *argv[])
{
	bool turn_issn_into_missing_year    = false;  // << mhmulati >>
	bool turn_every_entry_url_exception = true;   // << mhmulati >>
	bool keep_url_only_if_no_doi        = true;   // << mhmulati >> 
	bool keep_annote                    = false;   // << mhmulati >>
	bool keep_abstract                  = false;   // << mhmulati >>
	

	// MODIFY THIS BLOCK TO ADD/REMOVE BIB ENTRY TYPES THAT
	// SHOULD HAVE A URL DISPLAYED. BY DEFAULT, ALL URLS
	// ARE REMOVED FROM THE BIB-FILE.
	// TO ADD AN EXCEPTION:
	//	1)	INCREMENT NUM_URL_EXCEPTIONS
	//  2) 	APPEND THE NEW EXCEPTION TO THE LAST INDEX OF
	// 		URL_EXCEPTION_TYPES (WRITE WITHOUT THE '@' PREFIX).
	// TO REMOVE AN EXCEPTION:
	// 	1)	DECREMENT NUM_URL_EXCEPTIONS
	// 	2) 	REMOVE EXCEPTION STRING WRITTEN TO URL_EXCEPTION_TYPES
	// 	3) 	CORRECT INDICES OF REMAINING EXCEPTIONS SO THAT THEY
	// 		GO FROM 0 TO (NUM_URL_EXCEPTIONS-1)
	// NOTE: MENDELEY EXPORTS A "WEB PAGE" ENTRY AS "misc"
	const int NUM_URL_EXCEPTIONS = 2;
	const char *URL_EXCEPTION_TYPES[NUM_URL_EXCEPTIONS];
	URL_EXCEPTION_TYPES[0] = "misc";
	URL_EXCEPTION_TYPES[1] = "unpublished";
	// END OF USER-MODIFIED URL EXCEPTION BLOCK
	
	int curException;
	bool bUrlException;
	char bibType[BIB_TYPE_MAX];
	
	char INPUT_DEFAULT[]  = "library.bib";
	char OUTPUT_DEFAULT[] = "library_fixed.bib";

	char * inputName;
	char * outputName;
	
	FILE * inputFile;
	FILE * outputFile;
	
	unsigned long fileLength;
	unsigned long temp;  // Garbage variable for discarded file content length
	char * inputContent;
	char * outputContent;
	
	unsigned long curInputInd, curInputAnchorInd;
	
	// Bib-entry variables
	unsigned long numEntry = 0;
	char * curBibEntry;
	unsigned long curBibInd, curBibLength, indEOL;
	
	// Year-tracking variables
	bool bHasYear;          // Current entry defined the year
	bool bHasISSN;          // Current entry defined the year
	unsigned long issnInd;  // Index of the issn in the current entry.
	                        // This entry is renamed to the year if year is not defined
	bool has_doi;           // << mhmulati >>
	
	// Timer variables
	clock_t startTime, endTime;
	
	// Read in output filename if defined
	if(argc > 2)
	{
		inputName = stringWrite(argv[2]);
	}
	else
	{
		inputName = stringWrite(INPUT_DEFAULT);
	}
	
	// Read in input filename if defined
	if(argc > 1)
	{
		outputName = stringWrite(argv[1]);
	}
	else
	{
		outputName = stringWrite(OUTPUT_DEFAULT);
	}
	
	// Open input file
	inputFile = fopen(inputName, "r");
	if(inputFile == NULL)
	{
		fprintf(stderr,"ERROR: Input file \"%s\" not found.\n",inputName);
		exit(EXIT_FAILURE);
	}
	printf("Successfully opened input file at \"%s\".\n", inputName);
	
	// Read in contents of input file
	fseek(inputFile, 0, SEEK_END);
	fileLength = ftell(inputFile);
	fseek(inputFile,0,SEEK_SET);
	inputContent = malloc(fileLength + 1);
	outputContent = malloc(fileLength + 1);  // Output will be no longer than input
	if(inputContent == NULL
		|| outputContent == NULL)
	{
		fprintf(stderr,"ERROR: Memory could not be allocated to store the input file contents.\n");
		exit(EXIT_FAILURE);
	}
	temp = fread(inputContent,1,fileLength,inputFile);
	fclose(inputFile);
	printf("Successfully read and closed input file.\n");
	
	//
	// Scan and fix bib entries
	//
	numEntry = 0;
	startTime = clock();
	curInputInd = 0;
	curInputAnchorInd = 0;
	outputContent[0] = '\0';  // Initialize output string as empty
	while(true)
	{
		// Find start of next entry
		while(inputContent[curInputInd] != '@')
		{
			if(inputContent[curInputInd] == '\0')
				break;  // Reached EOF. No more entries to scan
			else
				curInputInd++;
		}
		
		if(inputContent[curInputInd] == '\0')
			break;
	
		curInputAnchorInd = curInputInd++;
		
		// Find end of entry
		while(true)
		{
			if((inputContent[curInputInd] == '}'
				&& inputContent[curInputInd-1] == '\n'
				&& (inputContent[curInputInd+1] == '\n'
				|| inputContent[curInputInd+1] == '\0'))
				|| inputContent[curInputInd] == '\0')
				break;  // Reached end of current entry (or EOF)
			else
				curInputInd++;
			
		}
		
		if(inputContent[curInputInd] == '\0')
			break;
		
		// Current entry goes from inputContent[curInputAnchorInd]
		// to inputContent[curInputInd]+1
		curBibLength = curInputInd-curInputAnchorInd+2;
		curBibEntry = malloc((curBibLength + 1)*sizeof(char));
		if(curBibEntry == NULL)
		{
			fprintf(stderr,"ERROR: Memory could not be allocated to copy bib entry %lu.\n", numEntry);
			exit(EXIT_FAILURE);
		}
		for(curBibInd = 0; curBibInd < curBibLength; curBibInd++)
		{
			curBibEntry[curBibInd] = inputContent[curInputAnchorInd+curBibInd];
		}
		curBibEntry[curBibInd] = '\0';
		
		// curBibEntry is now a valid substring of the original input file
		// Apply fixes as necessary
		curBibInd = 1;  // We know first character is '@'
		
		// Check URL exception types
		bUrlException = false;
		while(curBibEntry[curBibInd] != '{'
			&& curBibInd < BIB_TYPE_MAX)
		{
			bibType[curBibInd-1] = curBibEntry[curBibInd];
			curBibInd++;
		}
		bibType[curBibInd-1] = '\0';
		
		for(curException = 0; curException < NUM_URL_EXCEPTIONS; curException++)
		{
			if(!strcmp(bibType,URL_EXCEPTION_TYPES[curException]))
			{
				bUrlException = true;  // Current type of entry needs to keep URL
				break;
			}
		}

		bUrlException = bUrlException || turn_every_entry_url_exception;  // << mhmulati >>
		
		bHasYear = false;
		bHasISSN = false;
		has_doi  = false;  // << mhmulati >>
		// Scan Remainder of entry
		while(curBibEntry[curBibInd] != '\0')
		{
			if(curBibEntry[curBibInd] == '\n')
			{
				// We're at the start of a line in the current bib entry
				// Scan ahead to see if its an entry that we need to fix
				if(!strncmp(&curBibEntry[curBibInd+1], "month =",7))
				{
					// Next line lists month. Format should be mmm
					// and not {mmm}
					if(curBibEntry[curBibInd+9] == '{'
						&& curBibEntry[curBibInd+13] == '}')
					{
						curBibEntry[curBibInd+9] = curBibEntry[curBibInd+10];
						curBibEntry[curBibInd+10] = curBibEntry[curBibInd+11];
						curBibEntry[curBibInd+11] = curBibEntry[curBibInd+12];
						// Delete offsets 12 and 13
						memmove(&curBibEntry[curBibInd+12], &curBibEntry[curBibInd+14],
							curBibLength - curBibInd-13);
						curBibLength -= 2;
					}
				}
				else if(!strncmp(&curBibEntry[curBibInd+1], "title =",7))
				{
					// Title is supposed to be surrounded by 1 set of braces and not 2
					// Remove extra set of curly braces
					indEOL = findEndOfLine(curBibEntry, curBibInd+1);
					// Shift title over extra opening curly brace
					memmove(&curBibEntry[curBibInd+10], &curBibEntry[curBibInd+11],
						indEOL - curBibInd-13);
					// Shift remaining text over extra closing curly brace
					memmove(&curBibEntry[indEOL-3], &curBibEntry[indEOL-1],
						curBibLength - indEOL + 2);
					curBibLength -= 2;
				}
				else if((!keep_annote && !strncmp(&curBibEntry[curBibInd+1], "annote =",8)) || (!keep_abstract && !strncmp(&curBibEntry[curBibInd+1], "abstract =",10)))  // << mhmulati >>
				{
					// Entry has an annotation or abstract. Erase the whole field
					indEOL = findEndOfField(curBibEntry, curBibInd+1);
					memmove(&curBibEntry[curBibInd+1], &curBibEntry[indEOL+1],
						curBibLength - indEOL + 1);
					curBibLength -= indEOL - curBibInd;
					curBibInd--;  // Correct index so that line after annote is read correctly
				}
				else if(!strncmp(&curBibEntry[curBibInd+1],"doi =",5))  // << mhmulati >>
				{                                                       // << mhmulati >>
					has_doi = true;                                 // << mhmulati >>
				}                                                       // << mhmulati >>
				else if(!strncmp(&curBibEntry[curBibInd+1], "file =",6))
				{
					// Entry has a filename. Erase the whole line
					indEOL = findEndOfLine(curBibEntry, curBibInd+1);
					memmove(&curBibEntry[curBibInd+1], &curBibEntry[indEOL+1],
						curBibLength - indEOL + 1);
					curBibLength -= indEOL - curBibInd;
					curBibInd--;  // Correct index so that line after filename is read correctly
				}
				else if(!strncmp(&curBibEntry[curBibInd+1], "url =",5))                         // << mhmulati >>
				{
					// note that doi comes (alfabetically) before the url in the input bib  // << mhmulati >>
					if(!bUrlException || 
					   (bUrlException && keep_url_only_if_no_doi && has_doi))               // << mhmulati >>
					{                                                                       // << mhmulati >>
						// Entry has a URL but it should be removed. Erase the whole line
						indEOL = findEndOfLine(curBibEntry, curBibInd+1);
						memmove(&curBibEntry[curBibInd+1], &curBibEntry[indEOL+1],
							curBibLength - indEOL + 1);
						curBibLength -= indEOL - curBibInd;
						curBibInd--;  // Correct index so that line after URL is read correctly
					}                                                                       // << mhmulati >>
				}
				else if(!strncmp(&curBibEntry[curBibInd+1], "year =",6))
				{
					// This entry defines the year
					bHasYear = true;
				}
				else if(!strncmp(&curBibEntry[curBibInd+1], "issn =",6))
				{
					// Record line where issn starts in case we need to rename it to the year
					bHasISSN = true;
					issnInd = curBibInd + 1;
				}
			}
			else if(!strncmp(&curBibEntry[curBibInd], "{\\{}",4))
			{
				// We have an incorrectly formatted opening curly brace
				// Remove 3 characters of memory
				memmove(&curBibEntry[curBibInd+1], &curBibEntry[curBibInd+4],
					curBibLength - curBibInd-2);
				curBibLength -= 3;
			}
			else if(!strncmp(&curBibEntry[curBibInd], "{\\}}",4))
			{
				// We have an incorrectly formatted closing curly brace
				// Remove 3 characters of memory
				curBibEntry[curBibInd] = '}';
				memmove(&curBibEntry[curBibInd+1], &curBibEntry[curBibInd+4],
					curBibLength - curBibInd-2);
				curBibLength -= 3;
			}
			
			curBibInd++;
		}
		
		if(turn_issn_into_missing_year && !bHasYear && bHasISSN)  // << mhmulati >>
		{
			// This entry does not define the year. Rename the issn to the year
			curBibEntry[issnInd] = 'y';
			curBibEntry[issnInd+1] = 'e';
			curBibEntry[issnInd+2] = 'a';
			curBibEntry[issnInd+3] = 'r';
		}
		
		// Write fixed entry to output string
		strcat(outputContent, curBibEntry);
			
		numEntry++;
	}
	
	endTime = clock();
	printf("Entry fixing took %f seconds\n", (double) (endTime-startTime)/CLOCKS_PER_SEC);
	
	// Write output string to output file
	if((outputFile = fopen(outputName, "w")) == NULL)
	{
		fprintf(stderr,"ERROR: Cannot create output file \"%s\".\n",outputName);
		exit(EXIT_FAILURE);
	}
	printf("Successfully created output file at \"%s\".\n", outputName);
	
	fprintf(outputFile, "%s", outputContent);
	fclose(outputFile);
	printf("Successfully wrote and closed output file with %lu entries.\n", numEntry);
	
	// Cleanup
	free(inputContent);
	free(inputName);
	free(outputName);
	free(curBibEntry);
	
	return 0;
}

// Allocate memory for a string
char * stringAllocate(long stringLength)
{
	char * string = malloc(stringLength+1);
	if(string == NULL)
	{
		fprintf(stderr,"ERROR: Memory could not be allocated for string copy.\n");
		exit(EXIT_FAILURE);
	}
	
	return string;
}

// Copy string (with memory allocation)
char * stringWrite(char * src)
{
	char * string = stringAllocate(strlen(src));
	strcpy(string, src);
	
	return string;
}

// Find next end of line in current string
unsigned long findEndOfLine(char * str, unsigned long startInd)
{
	unsigned long curInd = startInd;
	while(str[curInd] != '\n')
		curInd++;
	
	return curInd;
}

// Find end of field in current string
unsigned long findEndOfField(char * str, unsigned long startInd)
{
	unsigned long curInd = startInd;
	while(strncmp(&str[curInd+1], "},\n",3))
		curInd++;
	
	return curInd+3;
}
