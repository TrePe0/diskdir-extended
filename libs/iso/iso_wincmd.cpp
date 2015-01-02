// iso_wincmd.cpp : Defines the entry point for the DLL application.
//

#define _INC_STDLIB // this define used to prevent declaration of malloc/free
                    // in standard windows headers
#include "iso_wincmd.h"
#include "iso.h"

#define DebugString(x)
#define assert( x )

#ifdef ZeroMemory
#undef ZeroMemory
#endif

#define ZeroMemory( ptr, size ) {for( int i = 0; i < size; i++ ) *((char*)(ptr) + i) = 0;}


static HANDLE heap = 0;

static void* malloc( size_t size )
{
    assert( heap );
    return HeapAlloc( heap, HEAP_ZERO_MEMORY, size );
}

static void free( void* block )
{
    assert( heap );
    if( block )
        HeapFree( heap, 0, block );
    return;
}


static void* realloc( void* block, size_t size )
{
    assert( heap );

    if( block && !size )
    {
        HeapFree( heap, 0, block );
        return 0;
    }
    else if( block && size )
        return HeapReAlloc( heap, HEAP_ZERO_MEMORY, block, size );
    else if( !block && size )
        return HeapAlloc( heap, HEAP_ZERO_MEMORY, size );
    else // !block && !size
        return 0;
}

static char* lstrrchr( char* str, int chr )
{
    assert( str );
    for( int i = lstrlen( str ); i >= 0; i-- )
        if( str[i] == chr )
            return str + i;
    return 0;
}

static int lmemfind( const char* ptr1, int len1, const char* ptr2, int len2 )
{
    if( len1 < len2 )
        return -1;
    int size = len1 - len2;
    for( int i = 0; i <= size; i++ )
    {
        bool equal = true;
        for( int j = 0; j < len2; j++ )
            if( ptr1[i + j] != ptr2[j] )
            {
                equal = false;
                break;
            }
        if( equal )
            return i;
    }

    return -1;
}

static int lstrcmpn( const char* str1, const char* str2, int len, bool casesensitive = true )
{
    if( !len )
        return 0;
    char* b1 = (char*)malloc( len + 2 );
    char* b2 = (char*)malloc( len + 2 );

    lstrcpyn( b1, str1, len + 1 );
    lstrcpyn( b2, str2, len + 1 );

    int res = casesensitive ? lstrcmp( b1, b2 ) : lstrcmpi( b1, b2 );
    free( b1 );
    free( b2 );
    return res;
}

static char* litoa( int num, int digits = 1 )
{
    static char buffer[100];
    int i;
    for( i = 0; i < sizeof( buffer ); i++ )
        buffer[i] = '0';
    for( i = 0; num > 0; i++, num /= 10 )
        buffer[sizeof( buffer ) - 1 - i] = (char)((num % 10) + '0');
    return buffer + sizeof( buffer ) - max( i, digits );
}

#define LOWDWORD(x) (DWORD)(x)
#define HIDWORD(x) (DWORD)((x) >> 32)

static DWORD ReadDataByPos( HANDLE file, LONGLONG position, DWORD size, void* data )
{
    assert( file != 0 && file != INVALID_HANDLE_VALUE );
    assert( data );

    LONG low = LOWDWORD( position );
    LONG high = HIDWORD( position );
    
    if( ((LONGLONG)SetFilePointer( file, low, &high, FILE_BEGIN ) + ((LONGLONG)high << 32)) != position )
        return 0;

    DWORD read;

    if( ReadFile( file, data, size, &read, 0 ) )
        return read;
    else
        return 0;
}

static LONGLONG GetBlockOffset( const IsoImage* image, DWORD block )
{
    return (LONGLONG)(DWORD)block * (WORD)image->RealBlockSize + (image->HeaderSize ? image->HeaderSize : image->DataOffset);
}

static DWORD ReadBlock( const IsoImage* image, DWORD block, DWORD size, void* data )
{
    //DebugString( "ReadBlock" );
    assert( image && block && size && data );

    return ReadDataByPos( image->hFile, GetBlockOffset( image, block ), size, data );
}

static int ScanBootSections( const IsoImage* image, LONGLONG offset, SectionHeaderEntry* firstHeader, SectionEntry* entries )
{
    assert( image );
    assert( firstHeader );

    int images = 0;

    SectionHeaderEntry header = *firstHeader;
    do
    {
        CatalogEntry entry;

        for( int i = 0; i < header.NumberOfSectionEntries; i++, offset += sizeof( entry ) )
        {
            if( ReadDataByPos( image->hFile, offset, sizeof( entry ), &entry ) )
            {
                if( entry.Entry.BootIndicator == 0x88 )
                {
                    if( entries )
                        entries[images] = entry.Entry;
                    images++;
                }

                if( entry.Entry.BootMediaType & (1 << 5) )
                { // next entry is extension
                    for( ; ReadDataByPos( image->hFile, offset, sizeof( entry ), &entry ) &&
                           entry.Extension.ExtensionIndicator == 0x44                     &&
                           entry.Extension.Bits & (5 << 1); offset += sizeof( entry ) );
                }
            }
            else
            {
                DebugString( "can't read SectionEntry" );
                return 0;
            }
        }
    } while( header.HeaderIndicator == 90 );
    return images;
}

const char ZipHeader[] = {'p', 'k'};
const char RarHeader[] = {'r', 'a', 'r'};
const char ExeHeader[] = {'M', 'Z'};

// if sharp == 1 then we should make only 1 iteration in loop
static DWORD GetVolumeDescriptor( const IsoImage* image, PrimaryVolumeDescriptorEx* descriptor,
                                  DWORD startblk = 0, bool sharp = false )
{
    assert( descriptor );
    assert( image );

    ZeroMemory( descriptor, sizeof( descriptor ) );

    bool step1 = false;

    for( DWORD i = startblk;
         !step1 &&
         ReadBlock( image, i, sizeof( descriptor->VolumeDescriptor ),
                    &descriptor->VolumeDescriptor ) == sizeof( descriptor->VolumeDescriptor );
         i++, step1 = sharp )
    {
        if( !lstrcmpn( (char*)descriptor->VolumeDescriptor.StandardIdentifier, CDSignature, sizeof(CDSignature)) &&
            descriptor->VolumeDescriptor.VolumeDescriptorType == 1 )
        {
            DWORD block = i;
            // Found it!
            DebugString("Found it");
            // trying to read root directory
            char* buffer = (char*)malloc( (WORD)descriptor->VolumeDescriptor.LogicalBlockSize );
            if( buffer )
            {
                if( ReadBlock( image, (DWORD)descriptor->VolumeDescriptor.DirectoryRecordForRootDirectory.LocationOfExtent,
                               (WORD)descriptor->VolumeDescriptor.LogicalBlockSize, buffer ) !=
                               (WORD)descriptor->VolumeDescriptor.LogicalBlockSize )
                {
                    free( buffer );
                    continue;
                }
                // ok, we can read root directory...
                free( buffer );
            }

            for( ;!descriptor->Unicode && i < (DWORD)descriptor->VolumeDescriptor.VolumeSpaceSize; i++ )
            {
                PrimaryVolumeDescriptor unicode;
                ZeroMemory( &unicode, sizeof( unicode ) );
                if( ReadBlock( image, i, sizeof( unicode ), &unicode ) != sizeof( unicode ) ||
                    lstrcmpn( (char*)unicode.StandardIdentifier, CDSignature, sizeof( CDSignature ) ) )
                    break;

                if( unicode.VolumeDescriptorType == 2                               &&
                    (DWORD)unicode.DirectoryRecordForRootDirectory.LocationOfExtent &&
                    (DWORD)unicode.DirectoryRecordForRootDirectory.DataLength )
                {
                    descriptor->Unicode = true;
                    descriptor->VolumeDescriptor = unicode;
                    DebugString("image.Unicode = TRUE");
                    break;
                }
            }

            assert( sizeof( BootRecordVolumeDescriptor ) == 0x800 );
            assert( sizeof( InitialEntry ) == 0x20 );
            assert( sizeof( SectionHeaderEntry ) == 0x20 );
            assert( sizeof( SectionEntry ) == 0x20 );
            assert( sizeof( SectionEntryExtension ) == 0x20 );
    
            BootRecordVolumeDescriptor boot;
            if( ReadBlock( image, block + 1, sizeof( boot ), &boot ) )
            { // check for boot image
                if( !boot.BootRecordIndicator &&
                    !lstrcmpn( (char*)boot.StandardIdentifier, CDSignature, sizeof( CDSignature ) ) &&
                    boot.VersionOfDescriptor == 1 &&
                    !lstrcmpn( (char*)boot.BootSystemIdentifier, TORITO, sizeof( TORITO ) ) )
                {
                    BootCatalog catalog;
                    if( ReadBlock( image, boot.BootCatalogPointer, sizeof( catalog ), &catalog ) )
                    {
                        DebugString( "reading boot catalog" );
                        if( (unsigned char)catalog.Validation.HeaderID == (unsigned char)1 &&
                            (short unsigned int)catalog.Validation.KeyWord == (short unsigned int)0xaa55 )
                        {
                            unsigned short sum = 0;
                            for( int i = 0; i < sizeof( catalog.Validation ) / sizeof( catalog.Validation.Checksum ); i++ )
                                sum = (unsigned short)(sum + *((unsigned short*)(&catalog.Validation) + i));
                            if( sum )
                            {
                                assert( "checksum is wrong :(" );
                            }
                            else
                            {
                                DebugString( "boot catalog detected" );
                                LONGLONG offset = GetBlockOffset( image, boot.BootCatalogPointer ) + sizeof( catalog );
                                int images = ScanBootSections( image, offset, &catalog.Entry[1].Header, 0 );
                                if( catalog.Entry[0].Initial.Bootable == 0x88 )
                                    images++;
                                if( images )
                                { // reading images
                                    descriptor->BootCatalog = (BootCatalog*)malloc( sizeof( BootCatalog ) +
                                                        max( 0, images - 2 ) * sizeof( CatalogEntry ) );
                                    assert( descriptor->BootCatalog );
                                    if( catalog.Entry[0].Initial.Bootable == 0x88 )
                                    {
                                        descriptor->BootCatalog->Entry[0] = catalog.Entry[0];
                                        ScanBootSections( image, offset, &catalog.Entry[1].Header,
                                                          &descriptor->BootCatalog->Entry[1].Entry );
                                    }
                                    else
                                        ScanBootSections( image, offset, &catalog.Entry[1].Header,
                                                          &descriptor->BootCatalog->Entry[0].Entry );
                                    descriptor->BootImageEntries = (DWORD)images;
                                }
                            }
                        }
                        else
                            DebugString( "wrong validation signature" );
                    }
                    else
                        DebugString( "can't read boot catalog from file" );
                }
            }

            return block;
        }
        else if(!lstrcmpn( (char*)descriptor->XBOXVolumeDescriptor.StandardIdentifier,
                           XBOXSignature, sizeof(XBOXSignature)) &&
                !lstrcmpn( (char*)descriptor->XBOXVolumeDescriptor.StandardIdentifier2,
                           XBOXSignature, sizeof(XBOXSignature)))
        {
            descriptor->XBOX = true;
            return i;
        }
    }

    return 0;
}

const DWORD SearchSize = 0x100000; 

static IsoImage* GetImage( const char* filename )
{
    DebugString( "GetImage" );
    if( !filename )
        return 0;

    IsoImage image;

    ZeroMemory( &image, sizeof( image ) );

    image.hFile = CreateFile( filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
    if( !image.hFile || image.hFile == INVALID_HANDLE_VALUE )
        return 0;

    assert( sizeof( PrimaryVolumeDescriptor ) == 0x800 ); // check for size of descriptor
    DWORD read;
    
    // check for zip or rar archives
    char ArcHeaderBuf[0x20];
    if( ReadDataByPos( image.hFile, 0, sizeof( ArcHeaderBuf ), ArcHeaderBuf ) != sizeof( ArcHeaderBuf ) ||
        !lstrcmpn( ExeHeader, ArcHeaderBuf, sizeof( ExeHeader ), false )                                ||
        !lstrcmpn( ZipHeader, ArcHeaderBuf, sizeof( ZipHeader ), false )                                ||
        !lstrcmpn( RarHeader, ArcHeaderBuf, sizeof( RarHeader ), false ) )
    {
        DebugString( "hmmm, this image can't be readed or it has zip or rar signature..." );
        CloseHandle( image.hFile );
        return 0;
    }
    
    PrimaryVolumeDescriptor descriptor;
    ZeroMemory( &descriptor, sizeof( descriptor ) );

    // Read the VolumeDescriptor (2k) until we find it, but only up to to 1MB
    for( ; image.DataOffset < SearchSize + sizeof( PrimaryVolumeDescriptor ); image.DataOffset += sizeof( PrimaryVolumeDescriptor ) )
    {
        char buffer[sizeof( PrimaryVolumeDescriptor ) + sizeof( CDSignature )];
        if( ReadDataByPos( image.hFile, 0x8000 + image.DataOffset,
                           sizeof( buffer ), buffer ) != sizeof( buffer ) )
        {
            // Something went wrong, probably EOF?
            DebugString("Could not read complete VolumeDescriptor block");
            CloseHandle( image.hFile );
            return 0;
        }

        int sig_offset = lmemfind( buffer, sizeof( buffer ), CDSignature, sizeof( CDSignature ) );
        if( sig_offset >= 0 )
            image.DataOffset += sig_offset - 1;
        else
        {
            if( image.DataOffset >= SearchSize )
            {
                // Just to make sure we don't read in a too big file, stop after 1MB.
                DebugString("Reached 1MB without descriptor");
                CloseHandle( image.hFile );
                return 0;
            }
            //image.DataOffset += sizeof( image.VolumeDescriptor );
            continue;
        }

        // Try to read a block
        read = ReadDataByPos( image.hFile, 0x8000 + image.DataOffset, sizeof( descriptor ), &descriptor );
        
        if( read != sizeof( descriptor ) )
        {
            // Something went wrong, probably EOF?
            DebugString("Could not read complete VolumeDescriptor block");
            CloseHandle( image.hFile );
            return 0;
        }

        if( lstrcmpn( (char*)descriptor.StandardIdentifier, CDSignature, sizeof( CDSignature ) ) == 0 &&
            descriptor.VolumeDescriptorType == 1 )
        {
            // Found it!
            DebugString("Found it");
            break;
        }
    }

    if( image.DataOffset >= SearchSize )
    {
        // Just to make sure we don't read in a too big file.
        DebugString("Reached 1MB without descriptor");
        CloseHandle( image.hFile );
        return 0;
    }

    image.RealBlockSize = (WORD)descriptor.LogicalBlockSize;

    // detect for next signature CDOO1
    char buffer[10000];
    if( ReadDataByPos( image.hFile, 0x8000 + image.DataOffset + 6, sizeof( buffer ), buffer ) )
    {
        int pos = lmemfind( buffer, sizeof( buffer ), CDSignature, sizeof( CDSignature ) );
        if( pos >= 0 )
            image.RealBlockSize = pos + sizeof( CDSignature );
        image.HeaderSize = (0x8000 + image.DataOffset) % image.RealBlockSize;
    }

    // check for strange nero format
    if( image.DataOffset + 0x8000 == image.RealBlockSize * 0xa6 + image.HeaderSize )
        image.HeaderSize += image.RealBlockSize * 0x96;
    else if( image.HeaderSize > 0xff )
        image.HeaderSize = 0;

    PrimaryVolumeDescriptorEx descriptorex;
    ZeroMemory( &descriptorex, sizeof( descriptorex ) );
    DWORD fblock = 0;

    for( image.DescriptorNum = 0;
         (fblock = GetVolumeDescriptor( &image, &descriptorex, fblock )) != 0;
         image.DescriptorNum++ )
    {
        image.VolumeDescriptors = (PrimaryVolumeDescriptorEx*)realloc( image.VolumeDescriptors,
            (image.DescriptorNum + 1) * sizeof( *image.VolumeDescriptors ) );
        image.VolumeDescriptors[image.DescriptorNum] = descriptorex;

        // checking for XBOX image inside of current ISO image
        DWORD xbox = fblock + 0x10;
        fblock += (DWORD)descriptorex.VolumeDescriptor.VolumeSpaceSize;
        xbox = GetVolumeDescriptor(&image, &descriptorex, xbox, true);
        if(xbox)
        {
            image.VolumeDescriptors = (PrimaryVolumeDescriptorEx*)realloc( image.VolumeDescriptors,
                (image.DescriptorNum + 2) * sizeof( *image.VolumeDescriptors ) );
            image.VolumeDescriptors[image.DescriptorNum + 1] = descriptorex;
            image.DescriptorNum++;
        }

        ZeroMemory( &descriptorex, sizeof( descriptorex ) );
    }

    IsoImage* pimage = (IsoImage*)malloc( sizeof( *pimage ) );
    assert( pimage );
    *pimage = image;

    return pimage;
}

static bool FreeImage( IsoImage* image )
{
    DebugString( "FreeImage" );
    assert( image );

    if( image->hFile && image->hFile != INVALID_HANDLE_VALUE )
        CloseHandle( image->hFile );

    if( image->DirectoryList )
        free( image->DirectoryList );

    for( DWORD i = 0; i < image->DescriptorNum && image->VolumeDescriptors; i++ )
        if( image->VolumeDescriptors[i].BootCatalog )
            free( image->VolumeDescriptors[i].BootCatalog );
    free( image->VolumeDescriptors );
    
    free( image );

    return true;
}

static bool LoadXBOXTree(IsoImage* image, PrimaryVolumeDescriptorEx* desc, const char* path,
                         unsigned int block, unsigned int size, Directory** dirs, DWORD* count)
{
    if(!block || !count || !size)
        return true;

    if( size > 10 * 1024 * 1024 )
        return false;

    char* data = (char*)malloc( size );
    assert( data );
    if( !data )
        return false;

    unsigned short* stack = (unsigned short*)malloc(size);
    int stackptr = 0;
    assert(stack);
    if( !stack )
    {
        free(data);
        return false;
    }

    if( ReadBlock(image, block, size, data) != size )
    {
        free(data);
        free(stack);
        return false;
    }

    DWORD offset = 0;
    bool result = true;

    do
    {
        XBOXDirectoryRecord* record = (XBOXDirectoryRecord*)(data + offset);
        Directory directory;
        ZeroMemory( &directory, sizeof( directory ) );
        directory.XBOXRecord = *record;
        directory.VolumeDescriptor = desc;

        lstrcpyn( directory.FileName, (char*)record->FileIdentifier,
                  min(record->LengthOfFileIdentifier + 1, sizeof(directory.FileName)));

        lstrcat( lstrcat( lstrcpy( directory.FilePath, path ), path[0] ? "\\" : "" ), directory.FileName );

        if( dirs )
            (*dirs)[*count] = directory;
        (*count)++;

        if( directory.XBOXRecord.FileFlags & XBOX_DIRECTORY )
            if( !LoadXBOXTree( image, desc, directory.FilePath,
                               directory.XBOXRecord.LocationOfExtent, directory.XBOXRecord.DataLength,
                               dirs, count ) )
                result = false;


        if(directory.XBOXRecord.RightSubTree) // push right ptr to stack
            stack[stackptr++] = directory.XBOXRecord.RightSubTree;

        offset = directory.XBOXRecord.LeftSubTree * 4;
        if(!offset && stackptr) // if we got last element in "line" - then get last element from stack
            offset = stack[--stackptr] * 4;
    } while(offset);

    free(stack);
    free(data);

    return result;
}


static bool LoadTree( IsoImage* image, PrimaryVolumeDescriptorEx* desc, const char* path, DirectoryRecord* root,
                      Directory** dirs, DWORD* count, bool boot = false )
{
    assert( image && root && count && desc );

    if(desc->XBOX)
        return LoadXBOXTree(image, desc, path, desc->XBOXVolumeDescriptor.LocationOfExtent,
                            desc->XBOXVolumeDescriptor.DataLength, dirs, count);

    //DebugString( "LoadTree" );
    DWORD block = (DWORD)root->LocationOfExtent;
    DWORD size = (DWORD)root->DataLength;

    if( !block || !count )
        return true;

    if( size > 10 * 1024 * 1024 )
        return false;

    char* data = (char*)malloc( size );
    assert( data );
    if( !data )
        return false;

    DWORD position = 0;
    for( DWORD k = 0; position < size; position += (WORD)desc->VolumeDescriptor.LogicalBlockSize, k++ )
    {
        DWORD s = min( size - position, (WORD)desc->VolumeDescriptor.LogicalBlockSize );
        if( ReadBlock( image, block + k, s, data + position ) != s )
        {
            free( data );
            return false;
        }
    }
    
    //if( ReadBlock( image, block, size, data ) != size )
    //{
    //    free( data );
    //    return false;
    //}

    bool result = true;
    
    DWORD offset = 0;
    DWORD num = 0;
    while( offset < size - 0x21 )
    {
        while( !data[offset] && offset < size - 0x21 )
            offset++;

        if( offset >= size - 0x21 )
            break;

        DirectoryRecord* record = (DirectoryRecord*)(data + offset);
        Directory directory;
        ZeroMemory( &directory, sizeof( directory ) );
        directory.Record = *record;
        directory.VolumeDescriptor = desc;

        if( (((int)record->LengthOfDirectoryRecord + 1) & ~1) <
            ((((int)sizeof( *record ) - (int)sizeof( *record->FileIdentifier ) + (int)record->LengthOfFileIdentifier) + 1) & ~1) )
            break;

        if( desc->Unicode && record->LengthOfFileIdentifier > 1 && num > 1 )
        {
            UINT codepage = CP_UTF8;
            int i;
            int length = record->LengthOfFileIdentifier;
            
            for( i = 0; i < record->LengthOfFileIdentifier; i++ )
            {
                if( record->FileIdentifier[i] < 32 )
                {
                    //codepage = CP_ACP;
                    codepage = GetACP();
                    length = record->LengthOfFileIdentifier / 2;
                    WCHAR* uname = (WCHAR*)record->FileIdentifier;
                    for( i = 0; i < (record->LengthOfFileIdentifier / 2); i++ )
                        uname[i] = (WORD)((uname[i] >> 8) + (uname[i] << 8));

                    WideCharToMultiByte( codepage, 0, (WCHAR*)record->FileIdentifier, length,
                                         directory.FileName, sizeof( directory.FileName ) - 1, 0, 0 );

                    break;
                }
            }

            if( codepage == CP_UTF8 )
                lstrcpyn( directory.FileName, (char*)record->FileIdentifier, record->LengthOfFileIdentifier + 1 );
            
            if( lstrrchr( directory.FileName, ';' ) )
                *lstrrchr( directory.FileName, ';' ) = 0;

            lstrcat( lstrcat( lstrcpy( directory.FilePath, path ), (path[0]) ? "\\" : "" ), directory.FileName );

            if( dirs )
                (*dirs)[*count] = directory;
            (*count)++;

            if( directory.Record.FileFlags & FATTR_DIRECTORY )
                /*if( !LoadTree( image, directory.FilePath, &directory.Record, dirs, count ) )
                {
                    free( data );
                    return false;
                }*/
                if( !LoadTree( image, desc, directory.FilePath, &directory.Record, dirs, count ) )
                    result = false;
        }
        else if( !desc->Unicode && record->LengthOfFileIdentifier > 0 && num > 1 )
        {
            lstrcpyn( directory.FileName, (char*)record->FileIdentifier,
                      min(record->LengthOfFileIdentifier + 1, sizeof(directory.FileName)));

            if( lstrrchr( directory.FileName, ';' ) )
                *lstrrchr( directory.FileName, ';' ) = 0;

            lstrcat( lstrcat( lstrcpy( directory.FilePath, path ), path[0] ? "\\" : "" ), directory.FileName );

            if( dirs )
                (*dirs)[*count] = directory;
            (*count)++;

            if( directory.Record.FileFlags & FATTR_DIRECTORY )
                /*if( !LoadTree( image, directory.FilePath, &directory.Record, dirs, count ) )
                {
                    free( data );
                    return false;
                }*/
                if( !LoadTree( image, desc, directory.FilePath, &directory.Record, dirs, count ) )
                    result = false;
        }

        offset += record->LengthOfDirectoryRecord;
        num++;
    }

    if( desc->BootImageEntries && boot )
    {
        if( dirs )
        {
            Directory* dir = (*dirs) + (*count);
            ZeroMemory( dir, sizeof( *dir ) );
            dir->VolumeDescriptor = desc;
            if( lstrlen( path ) )
            {
                lstrcpy( dir->FilePath, lstrcpy( dir->FileName, path ) );
                lstrcat( dir->FilePath, "\\" );
                lstrcat( dir->FileName, "\\" );
            }
            
            const char* boot_images = "boot.images";

            lstrcat( dir->FilePath, boot_images );
            lstrcat( dir->FileName, boot_images );
            dir->Record.FileFlags = FATTR_DIRECTORY | FATTR_HIDDEN;
        }
        (*count)++;
    }

    for( DWORD i = 0; i < desc->BootImageEntries && boot; i++ )
    {
        DebugString( "reading boot images" );
        if( dirs )
        {
            Directory* dir = (*dirs) + (*count);
            ZeroMemory( dir, sizeof( *dir ) );
            CatalogEntry* BootEntry = &desc->BootCatalog->Entry[i];
            char number[4] = "00";
            number[1] = (char)((i % 10) + '0');
            number[0] = (char)(((i / 10) % 10) + '0');
            
            char mediaType[50];
            const char* floppy = "floppy";
            const char* harddisk  = "harddisk";
            const char* no_emul = "no_emul";
            const char* img = ".";
            const char* ima = ".ima";
            //const char* img = ".img";

            switch( BootEntry->Entry.BootMediaType )
            {
            case 0:
                lstrcat( lstrcat( lstrcpy( dir->FileName, no_emul ), img ), number);
                dir->Record.DataLength = BootEntry->Entry.SectorCount * 0x200;
                break;
            case 1:
                lstrcat( lstrcat( lstrcat( lstrcpy( dir->FileName, floppy ), "_1.20."), number ), ima );
                dir->Record.DataLength = 0x50 * 0x2 * 0x0f * 0x200;
                break;
            case 2:
                lstrcat( lstrcat( lstrcat( lstrcpy( dir->FileName, floppy ), "_1.44."), number ), ima );
                dir->Record.DataLength = 0x50 * 0x2 * 0x12 * 0x200;
                break;
            case 3:
                lstrcat( lstrcat( lstrcat( lstrcpy( dir->FileName, floppy ), "_2.88."), number ), ima );
                dir->Record.DataLength = 0x50 * 0x2 * 0x24 * 0x200;
                break;
            case 4:
                {
                    lstrcat( lstrcat( lstrcpy( dir->FileName, harddisk ), img ), number );
                    MBR mbr;
                    if( ReadBlock( image, BootEntry->Entry.LoadRBA, 0x200, &mbr ) )
                        if( mbr.Signature == (unsigned short)0xaa55 )
                            dir->Record.DataLength = (mbr.Partition[0].start_sect + mbr.Partition[0].nr_sects) * 0x200;
                        else
                        {
                            DebugString( "hard disk signature doesn't match" );
                            dir->Record.DataLength = BootEntry->Entry.SectorCount * 0x200;
                        }
                    else
                    {
                        DebugString( "can't read MBR for hard disk emulation" );
                        dir->Record.DataLength = BootEntry->Entry.SectorCount * 0x200;
                    }
                }
                break;
            default:
                lstrcpy( mediaType, ".unknown" );
                dir->Record.DataLength = BootEntry->Entry.SectorCount * 0x200;
                break;
            }
            if( lstrlen( path ) )
                lstrcat( lstrcat( lstrcpy( dir->FilePath, path ), "\\boot.images\\" ), dir->FileName );
            else
                lstrcat( lstrcpy( dir->FilePath, "boot.images\\" ), dir->FileName );

            dir->Record.LocationOfExtent = BootEntry->Entry.LoadRBA;
            dir->VolumeDescriptor = desc;
        }
        (*count)++;
    }

    free( data );

    return result;
}

static bool LoadAllTrees( IsoImage* image, Directory** dirs, DWORD* count, bool boot = false )
{
    if( image->DescriptorNum < 2 )
        return LoadTree( image, image->VolumeDescriptors, "",
                         &image->VolumeDescriptors->VolumeDescriptor.DirectoryRecordForRootDirectory,
                         dirs, count, boot );
    else
    {
        bool result = false;
        int digs;
        int descnum = image->DescriptorNum;
        for( digs = 0; descnum; digs++, descnum /= 10 );
        for( DWORD i = 0; i < image->DescriptorNum; i++ )
        {
            char session[50] = "";
            if(!image->VolumeDescriptors[i].XBOX)
            {
                lstrcpy(session, "session");
                lstrcat( session, litoa( i + 1, digs ) );
            }
            //else
            //    lstrcpy(session, "XBOX");
            result |= LoadTree( image, image->VolumeDescriptors + i, session,
                                &image->VolumeDescriptors[i].VolumeDescriptor.DirectoryRecordForRootDirectory,
                                dirs, count, boot );
        }
        return result;
    }
}

HANDLE ISO_OpenArchive( tOpenArchiveData* ArchiveData )
{
	heap = HeapCreate( 0, 1024 * 1024, 0 );

	DebugString( "OpenArchive" );
    IsoImage* image = GetImage( ArchiveData->ArcName );
    return image;
}

// modified for DiskDirExtended purposes ReadHeader -> readHeaderEx
int ISO_ReadHeaderEx( HANDLE hArcData, tHeaderDataEx* HeaderData )
{
    //DebugString( "ReadHeader" );

    if( !hArcData || !HeaderData )
        return E_BAD_DATA;

    ZeroMemory(HeaderData, sizeof(*HeaderData));
    
    IsoImage* image = (IsoImage*)hArcData;
    if( !image->DirectoryList )
    {
        DWORD count = 0;
        if( !LoadAllTrees( image, 0, &count, true ) && !count )
            return E_BAD_ARCHIVE;
        if( count )
        {
            int size = sizeof( Directory ) * count;
            image->DirectoryList = (Directory*)malloc( size );
            ZeroMemory(image->DirectoryList, size);
            if( !LoadAllTrees( image, &image->DirectoryList, &image->DirectoryCount, true )  && !image->DirectoryCount )
                return E_BAD_ARCHIVE;
        }
    }

    if( image->Index >= image->DirectoryCount )
        return E_END_ARCHIVE;

    Directory* directory = image->DirectoryList + image->Index;
    lstrcpy( HeaderData->FileName, directory->FilePath );

    if(!directory->VolumeDescriptor->XBOX)
    {
        SYSTEMTIME time;
        FILETIME   ftime;
        ZeroMemory( &time, sizeof( time ) );
        time.wYear   = (WORD)(directory->Record.RecordingDateAndTime.Year + 1900);
        time.wMonth  = directory->Record.RecordingDateAndTime.Month;
        time.wDay    = directory->Record.RecordingDateAndTime.Day;
        time.wHour   = directory->Record.RecordingDateAndTime.Hour;
        time.wMinute = directory->Record.RecordingDateAndTime.Minute;
        time.wSecond = directory->Record.RecordingDateAndTime.Second;

        SystemTimeToFileTime( &time, &ftime );
        FileTimeToDosDateTime( &ftime, ((WORD*)&HeaderData->FileTime) + 1, (WORD*)&HeaderData->FileTime );

        HeaderData->FileAttr = FA_ARCHIVE;
        HeaderData->FileAttr |= (directory->Record.FileFlags & FATTR_DIRECTORY) ? FA_DIRECTORY : 0;
        HeaderData->FileAttr |= (directory->Record.FileFlags & FATTR_HIDDEN) ? FA_HIDDEN : 0;
        HeaderData->PackSize = HeaderData->UnpSize = (int)directory->Record.DataLength;
        HeaderData->PackSizeHigh = HeaderData->UnpSizeHigh = (int)_rotr64(directory->Record.DataLength, 32);
    }
    else
    {
        HeaderData->FileAttr = FA_ARCHIVE;
        HeaderData->FileAttr |= (directory->XBOXRecord.FileFlags & XBOX_DIRECTORY) ? FA_DIRECTORY : 0;
        HeaderData->FileAttr |= (directory->XBOXRecord.FileFlags & XBOX_HIDDEN) ? FA_HIDDEN : 0;
        HeaderData->PackSize = HeaderData->UnpSize = (int)directory->XBOXRecord.DataLength;
        HeaderData->PackSizeHigh = HeaderData->UnpSizeHigh = (int)_rotr64(directory->XBOXRecord.DataLength, 32);
    }

    return 0;
}

int ISO_ProcessFile(HANDLE hArcData)
{
    //DebugString( "ProcessFile" );
    if( !hArcData )
        return E_BAD_DATA;

    IsoImage* image = (IsoImage*)hArcData;

    bool result = true;
    image->Index++;
    return result ? 0 : E_EWRITE;
}

int ISO_CloseArchive( HANDLE hArcData )
{
    DebugString( "CloseArchive" );
    FreeImage( (IsoImage*)hArcData );

	if( heap ) {
		HeapDestroy( heap );
		heap = 0;
	}
    return 0;
}
