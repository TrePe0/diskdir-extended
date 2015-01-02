/************************************************************************
  Filename   : IniFile.CPP
  Version    : 0.51
  Author(s)  : Carsten Breuer
 --[ Description ]-------------------------------------------------------

  This file contains a complete interface to read and write ini files
  like windows do it. I have ported it to CPP.
  This class have a simple test routine at the end of this file. To make
  a executable file define INIFILE_TEST_THIS_FILE after the includes

  --[ History ] ----------------------------------------------------------
	
  0.10: Original file by Carsten Breuer. First beta version.
  0.20: Ported to a C++ class
  0.30: Linux "\n" instead "\r\n" added. Thanks to Dieter Engelbrecht
  0.40: Problem with WriteString fixed.
  0.50: Problem with file pointer solved
  0.51: We better do smaller steps now. I have reformated to tab4. Sorry 
        New function DeleteKey added. Thanks for the guy who post this.
		I have also renamed the pointer to be sonsinstent with IniFile.Cpp.

  PLEASE update GetVersion!

 --[ How to compile ]----------------------------------------------------

  This file was developed under DJGPP and Rhide. If you are familiar with
  Borland C++ 3.1, you will feel like at home ;-)).
  Both tools are free software.

  Downloads at: http://www.delorie.com/djgpp


 --[ Where to get help/information ]-------------------------------------

  The author   : C.Breuer@OpenWin.de

 --[ License ] ----------------------------------------------------------

  LGPL (Free for private and comercial use)

  This program is free software; you can redistribute it and/or
  modify it under the terms of the GNU Library General Public License
  as published by the Free Software Foundation; either version 2
  of the License, or (at your option) any later version.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU Library General Public License for more details.

  You should have received a copy of the GNU Library General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

 ------------------------------------------------------------------------
  Copyright (c) 2000 Carsten Breuer
/************************************************************************/

// Standard Lib
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "inifile.h"
// #define INIFILE_TEST_THIS_FILE
#ifdef INIFILE_TEST_THIS_FILE
#include <conio.h>	// Only for test: Used :  kbhit
#endif

/*=========================================================================
   strupr -de-
  -------------------------------------------------------------------------
   Job : String to Uppercase 22.03.2001 Dieter Engelbrecht dieter@wintop.net
/*========================================================================*/
#ifdef DONT_HAVE_STRUPR
/* DONT_HAVE_STRUPR is set when INI_REMOVE_CR is defined */
void strupr( char *str )
{
    // We dont check the ptr because the original also dont do it.
    while (*str != 0)
    {
        if ( islower( *str ) )
        {
		     *str = toupper( *str );
        }
        str++;
	}
}
#endif

/*=========================================================================
   CIniFile : The constructor
*========================================================================*/
CIniFile::CIniFile (void)
{
	m_pEntry      = NULL;
	m_pCurEntry   = NULL;
	m_result [0]  = 0;
	m_pIniFile    = NULL;
}

/*=========================================================================
   CIniFile : The destructor
*========================================================================*/
CIniFile::~CIniFile (void)
{
	FreeAllMem ();
}

/*=========================================================================
   CIniFile : GetVersion
   Info     : The version is BCD coded. It maintain the major version in
              the upper 8 bits and the minor in the lower.
              0x0120 means version 1.20
*========================================================================*/
UINT CIniFile::GetVersion (void)
{
	return 0x0030;
}



/*=========================================================================
   OpenIniFile
  -------------------------------------------------------------------------
   Job : Opens an ini file or creates a new one if the requested file
         doesnt exists.
/*========================================================================*/
BOOL CIniFile::OpenIniFile (CCHR * FileName)
{
	char   Str [8196];
	char   *pStr;
	struct ENTRY *pEntry;

	FreeAllMem ();

	if (FileName == NULL)                             { return FALSE; }
	if ((m_pIniFile = fopen (FileName, "r")) == NULL) { return FALSE; }

	while (fgets (Str, 8196, m_pIniFile) != NULL)
	{
		pStr = strchr (Str, '\n');
		if (pStr != NULL) { *pStr = 0; }
		pEntry = MakeNewEntry ();
		if (pEntry == NULL) { return FALSE; }

		#ifdef INI_REMOVE_CR
		Len = strlen(Str);
		if ( Len > 0 )
		{
			if ( Str[Len-1] == '\r' )
			{
				Str[Len-1] = '\0';
			}
		}
		#endif

		pEntry->pText = (char *)malloc (strlen (Str)+1);
		if (pEntry->pText == NULL)
		{
			FreeAllMem ();
			return FALSE;
		}
		strcpy (pEntry->pText, Str);
		pStr = strchr (Str,';');
		if (pStr != NULL) { *pStr = 0; } /* Cut all comments */
		if ( (strstr (Str, "[") > 0) && (strstr (Str, "]") > 0) ) /* Is Section */
		{
			pEntry->Type = tpSECTION;
		}
		else
		{
			if (strstr (Str, "=") > 0)
			{
				pEntry->Type = tpKEYVALUE;
			}
			else
			{
				pEntry->Type = tpCOMMENT;
			}
		}
		m_pCurEntry = pEntry;
	}
	fclose (m_pIniFile);
	m_pIniFile = NULL;
	return TRUE;
}

/*=========================================================================
   CloseIniFile
  -------------------------------------------------------------------------
   Job : Closes the ini file without any modifications. If you want to
         write the file use WriteIniFile instead.
/*========================================================================*/
void CIniFile::CloseIniFile (void)
{
	FreeAllMem ();
	if (m_pIniFile != NULL)
	{
		fclose (m_pIniFile);
		m_pIniFile = NULL;
	}
}


/*=========================================================================
   WriteIniFile
  -------------------------------------------------------------------------
   Job : Writes the iniFile to the disk and close it. Frees all memory
         allocated by WriteIniFile;
/*========================================================================*/
bool CIniFile::WriteIniFile (const char *pFileName)
{
	struct ENTRY *pEntry = m_pEntry;
	if (m_pIniFile != NULL)
	{
		fclose (m_pIniFile);
	}
	if ((m_pIniFile = fopen (pFileName, "wb")) == NULL)
	{
		FreeAllMem ();
		return FALSE;
	}

	while (pEntry != NULL)
	{
		if (pEntry->Type != tpNULL)
		{
#			ifdef INI_REMOVE_CR
			fprintf (m_pIniFile, "%s\n", pEntry->pText);
#			else
			fprintf (m_pIniFile, "%s\r\n", pEntry->pText);
#			endif
		}
		pEntry = pEntry->pNext;
	}

	fclose (m_pIniFile);
	m_pIniFile = NULL;
	return TRUE;
}


/*=========================================================================
   WriteString : Writes a string to the ini file
*========================================================================*/
void CIniFile::WriteString (CCHR *pSection, CCHR *pKey, CCHR *pValue)
{
	EFIND List;
	char  Str [8196];

	if (ArePtrValid (pSection, pKey, pValue) == FALSE) { return; }
	if (FindKey  (pSection, pKey, &List) == TRUE)
	{
		sprintf (Str, "%s=%s%s", List.KeyText, pValue, List.Comment);
		FreeMem (List.pKey->pText);
		List.pKey->pText = (char *)malloc (strlen (Str)+1);
		strcpy (List.pKey->pText, Str);
	}
	else
	{
		if ((List.pSec != NULL) && (List.pKey == NULL)) /* section exist, Key not */
		{
			AddKey (List.pSec, pKey, pValue);
		}
		else
		{
			AddSectionAndKey (pSection, pKey, pValue);
		}
	}
}

/*=========================================================================
   WriteBool : Writes a boolean to the ini file
*========================================================================*/
void CIniFile::WriteBool (CCHR *pSection, CCHR *pKey, bool Value)
{
	char Val [2] = {'0',0};
	if (Value != 0) { Val [0] = '1'; }
	WriteString (pSection, pKey, Val);
}

/*=========================================================================
   WriteInt : Writes an integer to the ini file
*========================================================================*/
void CIniFile::WriteInt (CCHR *pSection, CCHR *pKey, int Value)
{
	char Val [12]; /* 32bit maximum + sign + \0 */
	sprintf (Val, "%d", Value);
	WriteString (pSection, pKey, Val);
}

/*=========================================================================
   WriteDouble : Writes a double to the ini file
*========================================================================*/
void CIniFile::WriteDouble (CCHR *pSection, CCHR *pKey, double Value)
{
	char Val [32]; /* DDDDDDDDDDDDDDD+E308\0 */
	sprintf (Val, "%1.10lE", Value);
	WriteString (pSection, pKey, Val);
}


/*=========================================================================
   ReadString : Reads a string from the ini file
*========================================================================*/
CCHR *CIniFile::ReadString (CCHR *pSection, CCHR *pKey, CCHR *pDefault)
{
	EFIND List;
	if (ArePtrValid (pSection, pKey, pDefault) == FALSE) { return pDefault; }
	if (FindKey  (pSection, pKey, &List) == TRUE)
	{
		strcpy (m_result, List.ValText);
		return m_result;
	}
	return pDefault;
}

/*=========================================================================
   ReadBool : Reads a boolean from the ini file
*========================================================================*/
BOOL CIniFile::ReadBool (CCHR *pSection, CCHR *pKey, BOOL Default)
{
	char Val [2] = {"0"};
	if (Default != 0) { Val [0] = '1'; }
	return (atoi (ReadString (pSection, pKey, Val))?1:0); /* Only allow 0 or 1 */
}

/*=========================================================================
   ReadInt : Reads a integer from the ini file
*========================================================================*/
int CIniFile::ReadInt (CCHR *pSection, CCHR *pKey, int Default)
{
	char Val [12];
	sprintf (Val,"%d", Default);
	return (atoi (ReadString (pSection, pKey, Val)));
}

/*=========================================================================
   ReadDouble : Reads a double from the ini file
*========================================================================*/
double CIniFile::ReadDouble (CCHR *pSection, CCHR *pKey, double Default)
{
	double Val;
	sprintf (m_result, "%1.10lE", Default);
	sscanf (ReadString (pSection, pKey, m_result), "%lE", &Val);
	return Val;
}

/*=========================================================================
   DeleteKey : Deletes an entry from the ini file
*========================================================================*/
bool CIniFile::DeleteKey (CCHR *pSection, CCHR *pKey)
{
	EFIND         List;
	struct ENTRY *pPrev;
	struct ENTRY *pNext;

	if (FindKey (pSection, pKey, &List) == TRUE)
	{
		pPrev = List.pKey->pPrev;
		pNext = List.pKey->pNext;
		if (pPrev)
		{
			pPrev->pNext=pNext;
		}
		if (pNext)
		{ 
			pNext->pPrev=pPrev;
		}
		FreeMem (List.pKey->pText);
		FreeMem (List.pKey);
		return TRUE;
	}
	return FALSE;
}




/* Here we start with our helper functions */

void CIniFile::FreeMem (void *pPtr)
{
	if (pPtr != NULL) { free (pPtr); }
}

void CIniFile::FreeAllMem (void)
{
	struct ENTRY *pEntry;
	struct ENTRY *pNextEntry;
	pEntry = m_pEntry;
	while (1)
	{
		if (pEntry == NULL) { break; }
		pNextEntry = pEntry->pNext;
		FreeMem (pEntry->pText); /* Frees the pointer if not NULL */
		FreeMem (pEntry);
		pEntry = pNextEntry;
	}
	m_pEntry    = NULL;
	m_pCurEntry = NULL;
}

struct ENTRY *CIniFile::FindSection (CCHR *pSection)
{
	char Sec  [130];
	char iSec [130];
	struct ENTRY *pEntry;
	sprintf (Sec, "[%s]", pSection);
	strupr  (Sec);
	pEntry = m_pEntry; /* Get a pointer to the first Entry */
	while (pEntry != NULL)
	{
		if (pEntry->Type == tpSECTION)
		{
			strcpy  (iSec, pEntry->pText);
			strupr  (iSec);
			if (strcmp (Sec, iSec) == 0)
			{
				return pEntry;
			}
		}
		pEntry = pEntry->pNext;
	}
	return NULL;
}

bool CIniFile::FindKey  (CCHR *pSection, CCHR *pKey, EFIND *pList)
{
	char Search [8196];
	char Found  [8196];
	char Text   [8196];
	char *pText;
	struct ENTRY *pEntry;
	pList->pSec        = NULL;
	pList->pKey        = NULL;
	pEntry = FindSection (pSection);
	if (pEntry == NULL) { return FALSE; }
	pList->pSec        = pEntry;
	pList->KeyText[0] = 0;
	pList->ValText[0] = 0;
	pList->Comment[0] = 0;
	pEntry = pEntry->pNext;
	if (pEntry == NULL) { return FALSE; }
	sprintf (Search, "%s",pKey);
	strupr  (Search);
	while (pEntry != NULL)
	{
		if ((pEntry->Type == tpSECTION) || /* Stop after next section or EOF */
			(pEntry->Type == tpNULL   ))
		{
			return FALSE;
		}
		if (pEntry->Type == tpKEYVALUE)
		{
			strcpy (Text, pEntry->pText);
			pText = strchr (Text, ';');
			if (pText != NULL)
			{
				strcpy (pList->Comment, pText);
				*pText = 0;
			}
			pText = strchr (Text, '=');
			if (pText != NULL)
			{
				*pText = 0;
				strcpy (pList->KeyText, Text);
				strcpy (Found, Text);
				*pText = '=';
				strupr (Found);
				/*            printf ("%s,%s\n", Search, Found); */
				if (strcmp (Found,Search) == 0)
				{
				   strcpy (pList->ValText, pText+1);
				   pList->pKey = pEntry;
				   return TRUE;
				}
			}
		}
		pEntry = pEntry->pNext;
	}
	return NULL;
}

BOOL CIniFile::AddItem (char Type, CCHR *pText)
{
	struct ENTRY *pEntry = MakeNewEntry ();
	if (pEntry == NULL) { return FALSE; }
	pEntry->Type = Type;
	pEntry->pText = (char*)malloc (strlen (pText) +1);
	if (pEntry->pText == NULL)
	{
		free (pEntry);
		return FALSE;
	}
	strcpy (pEntry->pText, pText);
	pEntry->pNext   = NULL;
	if (m_pCurEntry != NULL) { m_pCurEntry->pNext = pEntry; }
	m_pCurEntry    = pEntry;
	return TRUE;
}

bool CIniFile::AddItemAt (struct ENTRY *pEntryAt, char Mode, CCHR *pText)
{
	struct ENTRY *pNewEntry;
	if (pEntryAt == NULL)  { return FALSE; }
	pNewEntry = (struct ENTRY*) malloc (sizeof (ENTRY));
	if (pNewEntry == NULL) { return FALSE; }
	pNewEntry->pText = (char *) malloc (strlen (pText)+1);
	if (pNewEntry->pText == NULL)
	{
		free (pNewEntry);
		return FALSE;
	}
	strcpy (pNewEntry->pText, pText);
	if (pEntryAt->pNext == NULL) /* No following nodes. */
	{
		pEntryAt->pNext   = pNewEntry;
		pNewEntry->pNext  = NULL;
	}
	else
	{
		pNewEntry->pNext = pEntryAt->pNext;
		pEntryAt->pNext  = pNewEntry;
	}
	pNewEntry->pPrev = pEntryAt;
	pNewEntry->Type  = Mode;
	return TRUE;
}

bool CIniFile::AddSectionAndKey (CCHR *pSection, CCHR *pKey, CCHR *pValue)
{
	char Text [8196];
	sprintf (Text, "[%s]", pSection);
	if (AddItem (tpSECTION, Text) == FALSE) { return FALSE; }
	sprintf (Text, "%s=%s", pKey, pValue);
	return AddItem (tpKEYVALUE, Text)? 1 : 0;
}

void CIniFile::AddKey (struct ENTRY *pSecEntry, CCHR *pKey, CCHR *pValue)
{
	char Text [8196];
	sprintf (Text, "%s=%s", pKey, pValue);
	AddItemAt (pSecEntry, tpKEYVALUE, Text);
}

struct ENTRY *CIniFile::MakeNewEntry (void)
{
	struct ENTRY *pEntry;
	pEntry = (struct ENTRY *)malloc (sizeof (ENTRY));
	if (pEntry == NULL)
	{
		FreeAllMem ();
		return NULL;
	}
	if (m_pEntry == NULL)
	{
		m_pEntry = pEntry;
	}
	pEntry->Type  = tpNULL;
	pEntry->pPrev = m_pCurEntry;
	pEntry->pNext = NULL;
	pEntry->pText = NULL;
	if (m_pCurEntry != NULL)
	{
		m_pCurEntry->pNext = pEntry;
	}
	return pEntry;
}



#ifdef INIFILE_TEST_THIS_FILE
int main (void)
{
	CIniFile iFile;
	printf ("Hello World\n");
	iFile.OpenIniFile  ("Test.Ini");
	iFile.WriteString  ("Test", "Name", "Value");
	iFile.WriteString  ("Test", "Name", "OverWrittenValue");
	iFile.WriteString  ("Test", "Port", "COM1");
	iFile.WriteString  ("Test", "User", "James Brown jr.");
	iFile.WriteString  ("Configuration", "eDriver", "MBM2.VXD");
	iFile.WriteString  ("Configuration", "Wrap", "LPT.VXD");
	iFile.WriteInt     ("IO-Port", "Com", 2);
	iFile.WriteBool    ("IO-Port", "IsValid", 0);
	iFile.WriteDouble  ("TheMoney", "TheMoney", 67892.00241);
	iFile.WriteInt     ("Test"    , "ToDelete", 1234);
	iFile.WriteIniFile ("Test.Ini");
	printf ("Key ToDelete created. Check ini file. Any key to continue");
	while (!kbhit());
	iFile.OpenIniFile  ("Test.Ini");
	iFile.DeleteKey    ("Test"	  , "ToDelete");
	iFile.WriteIniFile ("Test.Ini");
	printf ("[Test] Name = %s\n", iFile.ReadString ("Test", "Name", "NotFound"));
	printf ("[Test] Port = %s\n", iFile.ReadString ("Test", "Port", "NotFound"));
	printf ("[Test] User = %s\n", iFile.ReadString ("Test", "User", "NotFound"));
	printf ("[Configuration] eDriver = %s\n", iFile.ReadString ("Configuration", "eDriver", "NotFound"));
	printf ("[Configuration] Wrap = %s\n", iFile.ReadString ("Configuration", "Wrap", "NotFound"));
	printf ("[IO-Port] Com = %d\n", iFile.ReadInt ("IO-Port", "Com", 0));
	printf ("[IO-Port] IsValid = %d\n", iFile.ReadBool ("IO-Port", "IsValid", 0));
	printf ("[TheMoney] TheMoney = %1.10lf\n", iFile.ReadDouble ("TheMoney", "TheMoney", 111));
	iFile.CloseIniFile ();
	return 0;
}
#endif

