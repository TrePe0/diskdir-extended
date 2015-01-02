// iso_wincmd.cpp : Defines the entry point for the DLL application.
//

#define _INC_STDLIB // this define used to prevent declaration of malloc/free
                    // in standard windows headers
#include "iso_wincmd.h"

#pragma warning(push)
#pragma warning(disable:4200)
#include "iso.h"
#pragma warning(pop)

#if defined DEBUG || defined DEBUG_MESSAGES
#define DebugString(x) OutputDebugString("iso_wincmd.cpp:"),OutputDebugString( __STR__(__LINE__)": " ),OutputDebugString( x ),OutputDebugString( "\n" )
#define __STR2__(x) #x
#define __STR__(x) __STR2__(x)
//#define assert( x ) if( !(x) ) DebugString( "Assertion "#x" in line "__STR__(__LINE__) )
#define assert( x ) if( !(x) ) MessageBox( 0, "Assertion "#x" in line "__STR__(__LINE__), "Assert", 0 )
#else
#define DebugString(x)
#define assert( x )
#endif

#ifdef ZeroMemory
#undef ZeroMemory
#endif

#define ZeroMemory( ptr, size ) {for( int i = 0; i < size; i++ ) *((char*)(ptr) + i) = 0;}
#define MemCopy(dst, src, size) {for(int i = 0; i < size; i++) *((char*)(dst) + i) = *((char*)(src) + i);}

#define sizeofa(x) (sizeof(x) / sizeof(*x))

static HANDLE heap = 0;

#ifndef DEBUG
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

#else
static void* malloc( DWORD size )
{
    assert( heap );

    char* ptr = (char*)HeapAlloc( heap, HEAP_ZERO_MEMORY, size + 12 );
    assert( ptr );
    if( !ptr )
        return 0;
    *(DWORD*)ptr = 0x12345678;
    *(DWORD*)(ptr + 4) = size;
    *(DWORD*)(ptr + size + 8) = 0x12345678;

    return ptr + 8;
}

void free( void* block )
{
    assert( heap );
    if( block )
    {
        // check for boundary error
        char* ptr = (char*)block;
        char* real_ptr = ptr - 8;
        if( *(DWORD*)(real_ptr) != 0x12345678 )
            DebugString( "error before pointer" );
        DWORD size = *(DWORD*)(real_ptr + 4);
        if( *(DWORD*)(ptr + size) != 0x12345678 )
            DebugString( "error after pointer" );

        HeapFree( heap, 0, real_ptr );
    }
    return;
}

static void* realloc( void* block, size_t size )
{
    assert( heap );

    if( block && !size )
    {
        free( block );
        return 0;
    }
    else if( block && size )
    {
        char* p1 = (char*)block;
        char* p2 = (char*)malloc( size );
        DWORD s1 = *(DWORD*)(p1 - 4);
        DWORD s2 = size;
        for( DWORD i = 0; i < min( s1, s2 ); i++ )
            p2[i] = p1[i];
        free( p1 );
        return p2;
    }
    else if( !block && size )
        return malloc( size );
    else // !block && !size
        return 0;
}

#endif //debug

/*
static char* lstrchr( char* str, int chr )
{
    assert( str );
    for( int i = 0; str[i]; i++ )
        if( str[i] == chr )
            return str + i;
    return 0;
}
*/

static char* lstrrchr( char* str, int chr )
{
    assert( str );
    for( int i = lstrlenA( str ); i >= 0; i-- )
        if( str[i] == chr )
            return str + i;
    return 0;
}

static wchar_t* lwcsrchr( wchar_t* str, int chr )
{
    assert( str );
    for( int i = lstrlenW( str ); i >= 0; i-- )
        if( str[i] == chr )
            return str + i;
    return 0;
}

/*
static char* lstrstr( const char* str1, const char* str2 )
{
    assert( str1 && str2 );

    int l1 = lstrlen( str1 );
    int l2 = lstrlen( str2 );
    for( int i = 0; i < l1 - l2; i++ )
    {
        int j;
        for( j = 0; j < l2; j++ )
           if( str1[i + j] != str2[j] )
               break;
        if( j >= l2 )
            return (char*)str1 + i;
    }
    return 0;
}
*/

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

    lstrcpynA( b1, str1, len + 1 );
    lstrcpynA( b2, str2, len + 1 );

    int res = casesensitive ? lstrcmpA( b1, b2 ) : lstrcmpiA( b1, b2 );
    free( b1 );
    free( b2 );
    return res;
}

static char* litoa( int num, int digits = 1 )
{
    static char buffer[100];
    int i;
    for( i = 0; i < sizeofa( buffer ); i++ )
        buffer[i] = '0';
    buffer[sizeofa(buffer) - 1] = 0;
    for( i = 0; num > 0; i++, num /= 10 )
        buffer[sizeofa( buffer ) - 2 - i] = (char)((num % 10) + '0');
    return buffer + sizeofa( buffer ) - max( i, digits ) - 1;
}

static wchar_t* litow( int num, int digits = 1 )
{
    static wchar_t buffer[100];
    int i;
    for( i = 0; i < sizeofa( buffer ); i++ )
        buffer[i] = '0';
    buffer[sizeofa(buffer) - 1] = 0;
    for( i = 0; num > 0; i++, num /= 10 )
        buffer[sizeofa( buffer ) - 2 - i] = (wchar_t)((num % 10) + '0');
    return buffer + sizeofa( buffer ) - max( i, digits ) - 1;
}

static int heur_high_word(__int64 v)
{ // here is heuristic function: in case if high word equal low rotated word - return 0,
  // for example: 12340000:00003412 - in this case we have duplicate of low-word value in high word, return 0
    const char* p = (char*)&v;

    for(int i = 0; i < sizeof(v) / 2; i++)
        if(p[i] != p[sizeof(v) - 1 - i])
            return (int)(v >> 32);

    return 0;
}

#define is_header(x) ((x) & 0x80)
#define is_mask(x, m, v) (((x) & (m)) == (v))

static int header_size(char s)
{
    if(is_mask(s, 0xE0, 0xC0)) // 011100000, 011000000
        return 1;
    if(is_mask(s, 0xF0, 0xE0)) // 011110000, 011100000
        return 2;
    if(is_mask(s, 0xF8, 0xF0)) // 011111000, 011110000
        return 3;
    if(is_mask(s, 0xFC, 0xF8)) // 011111100, 011111000
        return 4;
    return 0;
}

static bool is_utf8(const unsigned char* s, int len)
{ // try to detect UTF-8 string
    if(!s || !len)
        return false;

    char buffer[256];
    if(!GetEnvironmentVariableA("ISO_WCX_ENABLE_UTF8", buffer, sizeof(buffer)))
        return false;

    for(int i = 0; i < len;)
    {
        if((unsigned char)s[i] < ' ')
            return false; // can't be zero
        if(is_header(s[i]) && header_size(s[i]))
        {
            i++;
            int j;
            int size = header_size(s[i]);
            for(j = 0; j < size && i < len; j++, i++)
                if(!is_mask(s[i], 0xC0, 0x80)) // 011000000, 01000000
                    return false;
            if(j < size) // if string aborted at middle of utf-8 sequence - not utf-8
                return false;
        }
        else if(!is_header(s[i]))
            i++;
        else // if(is_header(s[i] && !header_size(s[i])))
            return false;
    }

    return true;
}

static bool ProcessUDF = false;

/* hack: in VC6.0 SetFilePointerEx is not defined */
typedef WINBASEAPI BOOL WINAPI SetFilePointerEx_t(
    HANDLE hFile,
    LARGE_INTEGER liDistanceToMove,
    PLARGE_INTEGER lpNewFilePointer,
    DWORD dwMoveMethod
    );

static SetFilePointerEx_t* SetFilePointerEx_f = 0;

/* commented out by TrePe
BOOL APIENTRY DllMain( HANDLE hinstDLL, DWORD fdwReason, LPVOID )
{
    if( !hinstDLL )
        return FALSE;

    switch( fdwReason )
    {
        case DLL_PROCESS_ATTACH:
            DebugString( "process attach" );
            //heap = GetProcessHeap();
            heap = HeapCreate( 0, 1024 * 1024, 0 );
            char buffer[256];
            if(GetEnvironmentVariableA("ISO_WCX_ENABLE_UDF", buffer, sizeof(buffer)))
                ProcessUDF = true;

            {
                HMODULE h = GetModuleHandleA("kernel32.dll");
                if(h)
                {
                    SetFilePointerEx_f = (SetFilePointerEx_t*)GetProcAddress(h, "SetFilePointerEx");
                    if(!SetFilePointerEx_f)
                    {
                        DebugString("can't locate SetFilePointerEx function in kernel32.dll. "
                            "using old stype file positioning (may fail on large images)");
                    }
                }
                else
                {
                    DebugString("no kernel32.dll... Linux forever? :)");
                }
            }

            break;
        case DLL_THREAD_ATTACH:
            DebugString( "thread attach" );
            break;
        case DLL_THREAD_DETACH:
            DebugString( "thread detach" );
            break;
        case DLL_PROCESS_DETACH:
            DebugString( "process detach" );
            if( heap )
            {
                HeapDestroy( heap );
                heap = 0;
            }
            break;
    }

    return TRUE;
}
*/
#define LOWDWORD(x) (DWORD)(x)
#define HIDWORD(x) (DWORD)((x) >> 32)

static LONGLONG GetFilePos(HANDLE file)
{
    assert(file && file != INVALID_HANDLE_VALUE);

    LONG low;
    LONG high = 0;

    low = SetFilePointer(file, 0, &high, FILE_CURRENT);

    LONGLONG res = low + ((LONGLONG)high << 32);
    return res;
}

static LONGLONG SetFilePos(HANDLE file, LONGLONG pos)
{
    assert(file && file != INVALID_HANDLE_VALUE);

    if(SetFilePointerEx_f)
    {
        LARGE_INTEGER p;
        LARGE_INTEGER ret;

        p.QuadPart = pos;

        if(SetFilePointerEx_f(file, p, &ret, FILE_BEGIN))
            return ret.QuadPart;
    }
    else
    { // use old style of file positioning... fail on some large images
        LONG low = LOWDWORD(pos);
        LONG high = HIDWORD(pos);

        low = SetFilePointer(file, low, &high, FILE_BEGIN);

        LONGLONG res = low + ((LONGLONG)high << 32);
        return res;
    }

    return 0;
}

static DWORD ReadDataByPos( HANDLE file, LONGLONG position, DWORD size, void* data )
{
    assert( file != 0 && file != INVALID_HANDLE_VALUE );
    assert( data );

    if(SetFilePos(file, position) != position )
        return 0;

    DWORD read = 0;

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
    assert( image && size && data );

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

    ZeroMemory( descriptor, sizeof( *descriptor ) );

    bool step1 = false;

    DWORD iterations = 0;
    DWORD max_iterations = 1024 * 1024 * 100 / image->RealBlockSize; // max 100 Mb between sessions
    
    for( DWORD i = startblk;
         !step1 && iterations < max_iterations &&
         ReadBlock( image, i, sizeof( descriptor->VolumeDescriptor ),
                    &descriptor->VolumeDescriptor ) == sizeof( descriptor->VolumeDescriptor );
         i++, step1 = sharp, iterations++ )
    {
        if( !lstrcmpn( (char*)descriptor->VolumeDescriptor.StandardIdentifier, CDSignature, sizeof(CDSignature)) &&
            descriptor->VolumeDescriptor.VolumeDescriptorType == 1 )
        {
            DWORD block = i;
            // Found it!
            DebugString("GetVolumeDescriptor: Found it");
            iterations = 0;
            // trying to read root directory
            char* buffer = (char*)malloc( (WORD)descriptor->VolumeDescriptor.LogicalBlockSize );
            if( buffer )
            {
                if( ReadBlock( image, (DWORD)descriptor->VolumeDescriptor.DirectoryRecordForRootDirectory.LocationOfExtent,
                               (WORD)descriptor->VolumeDescriptor.LogicalBlockSize, buffer ) !=
                               (WORD)descriptor->VolumeDescriptor.LogicalBlockSize )
                {
                    DebugString("GetVolumeDescriptor: can't read block for descriptor->VolumeDescriptor.DirectoryRecordForRootDirectory.LocationOfExtent");
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
                            for( int j = 0; j < sizeof( catalog.Validation ) / sizeof( catalog.Validation.Checksum ); j++ )
                                sum = (unsigned short)(sum + *((unsigned short*)(&catalog.Validation) + j));
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


static bool is_PrimaryVolumeDescriptor_UDF(IsoImage* image, char* buffer, int /*len*/, DWORD offset)
{
    PrimaryVolumeDescriptor* desc = (PrimaryVolumeDescriptor*)buffer;
    assert(image && desc && len);

    switch(image->UdfStage)
    {
    case no_udf:
        if(!lstrcmpn((char*)desc->StandardIdentifier, UDFBEA, sizeof(UDFBEA)))
        {
            image->UdfStage = udf_bea;
            image->UdfOffset = offset;
        }
        return false;
    case udf_bea:
        if(!lstrcmpn((char*)desc->StandardIdentifier, UDFNSR02, sizeof(UDFNSR02)) ||
           !lstrcmpn((char*)desc->StandardIdentifier, UDFNSR03, sizeof(UDFNSR03)))
        {
            image->UdfStage = udf_nsr;
            image->UdfBlockSize = offset - image->UdfOffset;
            image->UdfOffset = offset;
        }
        return false;
    case udf_nsr:
        if(!lstrcmpn((char*)desc->StandardIdentifier, UDFTEA, sizeof(UDFTEA)))
        {
            DWORD size = offset - image->UdfOffset;
            if(size == image->UdfBlockSize)
            { // ok, found UDF signature
                image->RealBlockSize = size;
                return true;
            }
        }
    }
    return false;
}

class PtrHolder
{
public:
    PtrHolder(void* ptr): m_ptr(ptr) {}
    ~PtrHolder() {if(m_ptr) free(m_ptr);}
protected:
    void* m_ptr;
};


static PartitionMap* GetUDFPartitionMap(LogicalVolumeDescriptor* logic, Uint16 num)
{
    if(!logic)
        return 0;

    PartitionMap* pmap;
    
    for(int j = 0, i = 0; j < (int)logic->MapTableLength; j += pmap->MapLength, i++)
    {
        pmap = (PartitionMap*)((char*)(&logic->PartitionMaps) + j);
        if(i == num)
            return pmap;
    }

    return 0;
}

static PartitionDescriptor* GetUDFPartitionDescriptor(UDFImage* image, Uint16 num)
{
    if(!image || !image->partition_desc_num || !image->PartitionDescriptor)
        return 0;

    for(int i = 0; i < image->partition_desc_num; i++)
        if(image->PartitionDescriptor[i]->PartitionNumber == num)
            return image->PartitionDescriptor[i];

    return 0;
}

static DWORD ReadUDFBlock(IsoImage* iso_image,
                          UDFImage* image, LogicalVolumeDescriptor* logic,
                          Uint32 logicalBlockNum, Uint16 partitionReferenceNum,
                          Uint32 len, void* data)
{
    if(!image || !iso_image || !logic || !len || !data)
        return 0;

    PartitionMap* pmap = GetUDFPartitionMap(logic, partitionReferenceNum);
    if(!pmap)
        return 0;

    if(pmap->MapType == 1)
    {
        PartitionMap1* pmap1 = (PartitionMap1*)pmap;
        PartitionDescriptor* pdesc = GetUDFPartitionDescriptor(image, pmap1->PartitionNumber);
        if(!pdesc)
            return 0;
        LONGLONG pos = ((LONGLONG)logicalBlockNum + pdesc->PartitionStartingLocation) * logic->LogicalBlockSize;
        return ReadDataByPos(iso_image->hFile, pos, (DWORD)len, data);
    }
    else
        return 0;

    // return 0;
}

static DWORD ReadUDFBlock(IsoImage* iso_image,
                          UDFImage* image, LogicalVolumeDescriptor* logic, lb_addr& lb,
                          Uint32 len, void* data)
{
    return ReadUDFBlock(iso_image, image, logic, lb.logicalBlockNum, lb.partitionReferenceNum, len, data);
}

static DWORD ReadUDFBlock(IsoImage* iso_image,
                          UDFImage* image, LogicalVolumeDescriptor* logic,
                          long_ad& ad, void* data)
{
    return ReadUDFBlock(iso_image, image, logic, ad.extLocation, ad.extLength, data);
}

#define FREE_ARRAY(ptr, num) if(ptr) {for(int i = 0; i < num; i++) if(ptr[i]) free(ptr[i]); free(ptr);}

static bool FreeUDFImage(UDFImage& image)
{
    FREE_ARRAY(image.PrimaryVolumeDescriptor_UDF, image.primary_num);
    FREE_ARRAY(image.LogicalVolumeDescriptor, image.logic_num);
    FREE_ARRAY(image.PartitionDescriptor, image.partition_desc_num);
    FREE_ARRAY(image.FileSetDescriptorEx, image.file_set_desc_num);
    return true;
}

#define ADD_DESCRIPTOR(image, counter, ptr, src, size)                                     \
    (image)->counter++;                                                                    \
    (image)->ptr = (ptr**)realloc((image)->ptr, (image)->counter * sizeof(*(image)->ptr)); \
    (image)->ptr[(image)->counter - 1] = (ptr*)malloc(size);                               \
    MemCopy((image)->ptr[(image)->counter - 1], src, size);

static IsoImage* ReadUDFStructure(IsoImage* image)
{
    DebugString("ReadUDFStructure");

    LONGLONG file_pos = GetFilePos(image->hFile);

    UDFImage udf_image;

    ZeroMemory(&udf_image, sizeof(udf_image));

    AnchorVolumeDescriptor anchor;
    image->DataOffset = 0;

    if(sizeof(anchor) != ReadBlock(image, AnchorVolumeDescriptorSector, sizeof(anchor), &anchor))
    {
        SetFilePos(image->hFile, file_pos);
        return 0;
    }

    char* buffer = (char*)malloc(anchor.MainVolumeDescriptorSequenceExtent.extLength);
    if(!buffer)
    {
        SetFilePos(image->hFile, file_pos);
        return 0;
    }

    PtrHolder __buffer_holder(buffer);
    
    ReadBlock(image, anchor.MainVolumeDescriptorSequenceExtent.extLocation,
                     anchor.MainVolumeDescriptorSequenceExtent.extLength, buffer);

    for(int i = 0; i * image->RealBlockSize < anchor.MainVolumeDescriptorSequenceExtent.extLength; i++)
    {
        tag* tg = (tag*)(buffer + i * image->RealBlockSize);
        switch(tg->TagIdentifier)
        {
        case TAG_IDENT_PVD:
            {
                PrimaryVolumeDescriptor_UDF* prim = (PrimaryVolumeDescriptor_UDF*)tg;
                ADD_DESCRIPTOR(&udf_image, primary_num, PrimaryVolumeDescriptor_UDF, prim, sizeof(*prim));
                break;
            }
        case TAG_IDENT_AVDP:
            {
                //AnchorVolumeDescriptor* anchor = (AnchorVolumeDescriptor*)tg;
                break;
            }
        case TAG_IDENT_IUVD:
            {
                //ImpUseVolumeDescriptor* imp_use = (ImpUseVolumeDescriptor*)tg;
                break;
            }
        case TAG_IDENT_PD:
            {
                PartitionDescriptor* pdesc = (PartitionDescriptor*)tg;
                ADD_DESCRIPTOR(&udf_image, partition_desc_num, PartitionDescriptor, pdesc, sizeof(*pdesc));
                break;
            }
        case TAG_IDENT_LVD:
            {
                LogicalVolumeDescriptor* logic = (LogicalVolumeDescriptor*)tg;
                ADD_DESCRIPTOR(&udf_image, logic_num, LogicalVolumeDescriptor, logic, sizeof(*logic) + logic->MapTableLength);

                /*PartitionMap* pmap = (PartitionMap*)logic->PartitionMaps;

                for(int j = 0; j < logic->MapTableLength; j += pmap->MapLength)
                {
                    pmap = (PartitionMap*)(logic->PartitionMaps + j);
                    if(pmap->MapType == 1)
                    {
                        PartitionMap1* pmap1 = (PartitionMap1*)pmap;
                        FileSetDescriptor fset;
                        ReadBlock(image, pdesc.PartitionStartingLocation + logic->LogicalVolumeContentsUse.extLocation.logicalBlockNum,
                                  sizeof(fset), &fset);

                        LONGLONG offset = GetBlockOffset(image, pdesc.PartitionStartingLocation + fset.RootDirectoryICB.extLocation.logicalBlockNum);

                        char buffer[0x800];

                        int ptr = 0;
                        for(int i = 0; i < 20; i++)
                        {
                            ReadDataByPos(image->hFile, offset + ptr, sizeof(buffer), buffer);

                            tag* tg = (tag*)(buffer);
                            switch(tg->TagIdentifier)
                            {
                            case TAG_IDENT_FID:
                                {
                                    FileIdentifierDescriptor* fid = (FileIdentifierDescriptor*)tg;
                                    ptr += (sizeof(*fid) + fid->LengthOfFileIdentifier + 1) & ~2;
                                    break;
                                }
                            case TAG_IDENT_FE:
                                {
                                    FileEntry* fentry = (FileEntry*)tg;
                                    ptr += 0x800;
                                    break;
                                }
                            default:
                                ptr = ptr & ~(0x800 - 1);
                                ptr += 0x800;
                                break;
                            }
                        }

                        FileEntry fentry;
                        ReadBlock(image, 0x101 + fset.RootDirectoryICB.extLocation.logicalBlockNum,
                                  sizeof(fentry), &fentry);
                    }
                    else if(pmap->MapType == 2)
                    {
                        PartitionMap2* pmap2 = (PartitionMap2*)pmap;
                    }
                } */

                break;
            }
        case TAG_IDENT_USD:
            {
                //UnallocatedSpaceDesc* unalloc = (UnallocatedSpaceDesc*)tg;
                break;
            }
        default:
            break;
        }
    }

    if(udf_image.partition_desc_num && udf_image.LogicalVolumeDescriptor)
    {
        FileSetDescriptorEx fset;

        for(int i = 0; i < udf_image.logic_num; i++)
        {
            long_ad ad = udf_image.LogicalVolumeDescriptor[i]->LogicalVolumeContentsUse;
            while(ad.extLength)
            {
                fset.LogicalVolumeDescriptor = udf_image.LogicalVolumeDescriptor[i];
                if(ReadUDFBlock(image, &udf_image, udf_image.LogicalVolumeDescriptor[i],
                             ad.extLocation,
                             sizeof(fset.FileSetDescriptor),
                             &fset.FileSetDescriptor) == sizeof(fset.FileSetDescriptor))
                {
                    ADD_DESCRIPTOR(&udf_image, file_set_desc_num, FileSetDescriptorEx, &fset, sizeof(fset));
             
                }
                else
                    break;

                ad = fset.FileSetDescriptor.NextExtent;
            }
        }
    }

    image->DescriptorNum++;
    image->VolumeDescriptors = (PrimaryVolumeDescriptorEx*)realloc(image->VolumeDescriptors,
        sizeof(*image->VolumeDescriptors) * image->DescriptorNum);
    image->VolumeDescriptors[image->DescriptorNum - 1].UDF = true;
    image->VolumeDescriptors[image->DescriptorNum - 1].UDFImage = udf_image;

    SetFilePos(image->hFile, file_pos);

    return image;
}

const DWORD SearchSize = 0x100000; 

static IsoImage* GetImage( const char* filename, const wchar_t* wfilename = 0 )
{
    DebugString( "GetImage" );
    if( !(filename || wfilename) )
        return 0;

    IsoImage image;

    ZeroMemory( &image, sizeof( image ) );

    if(filename)
        image.hFile = CreateFileA( filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
    else if(wfilename)
        image.hFile = CreateFileW( wfilename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, 0, 0 );
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
    for( ; image.DataOffset < SearchSize + sizeof( PrimaryVolumeDescriptor );
           image.DataOffset += sizeof( PrimaryVolumeDescriptor ) )
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

        if(ProcessUDF)
        {
            if(is_PrimaryVolumeDescriptor_UDF(&image, buffer, sizeof(buffer), image.DataOffset))
            {
                if(ReadUDFStructure(&image)) // TODO: add evaluation for failed reading UDF...
                {                            // ISSUE: how to restore file position???
                    IsoImage* pimage = (IsoImage*)malloc( sizeof( *pimage ) );
                    assert( pimage );
                    *pimage = image;

                    return pimage;
                }
            }
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

    if(!image.DescriptorNum)
    {
        DebugString("no volume descriptors... strange image...");
        return 0;
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

    if(image->VolumeDescriptors->UDF)
        FreeUDFImage(image->VolumeDescriptors->UDFImage);

    if( image->DirectoryList )
    {
        for(DWORD i = 0; i < image->DirectoryCount; i++)
        {
            if(image->DirectoryList[i].FileName)
                free(image->DirectoryList[i].FileName);
            if(image->DirectoryList[i].FilePath)
                free(image->DirectoryList[i].FilePath);
            if(image->DirectoryList[i].wFileName)
                free(image->DirectoryList[i].wFileName);
            if(image->DirectoryList[i].wFilePath)
                free(image->DirectoryList[i].wFilePath);
        }
        free( image->DirectoryList );
    }

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

		if(dirs)
        { // if dirs == 0 - we just calculating total number of records...
            directory.FileName = (char*)malloc((record->LengthOfFileIdentifier + 2) * sizeof(*directory.FileName));
		    assert(directory.FileName);
            if(!directory.FileName)
            {
                free(data);
                free(stack);
                return false;
            }
            lstrcpynA( directory.FileName, (char*)record->FileIdentifier, record->LengthOfFileIdentifier + 1);
            directory.FileName[record->LengthOfFileIdentifier] = 0;

		    int dirlen = lstrlenA(path) + 1 + lstrlenA(directory.FileName) + 1;
		    directory.FilePath = (char*)malloc(dirlen * sizeof(*directory.FilePath));
		    assert(directory.FilePath);
            if(!directory.FilePath)
            {
                free(data);
                free(stack);
                return false;
            }
            lstrcatA( lstrcatA( lstrcpyA( directory.FilePath, path ), path[0] ? "\\" : "" ), directory.FileName );

            (*dirs)[*count] = directory;
        }

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


static bool LoadUDFDir(IsoImage* image,  UDFImage* udf_image, LogicalVolumeDescriptor* logic, FileEntry* fentry,
                       const char* path, DirectoryRecord* root, Directory** dirs, DWORD* count)
{
    char* buffer = (char*)malloc(logic->LogicalBlockSize);
    if(!buffer)
        return false;
    
    PtrHolder __holder(buffer);

    long_ad* ad = (long_ad*)fentry->ExtendedAttributes;
    for(Uint32 i = 0; i < fentry->LengthOfAllocationDescriptors / sizeof(*ad); i++)
    {
        ReadUDFBlock(image, udf_image, logic, *ad, buffer);
        for(int index = 0; index < (int)ad->extLength;)
        {
            tag* tg = (tag*)(buffer + index);
            switch(tg->TagIdentifier)
            {
            case TAG_IDENT_FID:
                {
                    FileIdentifierDescriptor* fid = (FileIdentifierDescriptor*)tg;
                    index += (sizeof(*fid) + fid->LengthOfFileIdentifier + 1) & ~2;
                    break;
                }
            case TAG_IDENT_FE:
                {
                    FileEntry* fentry = (FileEntry*)tg;
                    LoadUDFDir(image, udf_image, logic, fentry, path, root, dirs, count);
                    index += sizeof(*fentry) + fentry->LengthOfAllocationDescriptors + fentry->LengthOfExtendedAttributes;
                    break;
                }
            default:
                index = index & ~(0x800 - 1);
                index += 0x800;
                break;
            }
        }
    }

    return false;
}

static bool LoadUDFTree(IsoImage* image, PrimaryVolumeDescriptorEx* desc, const char* path, DirectoryRecord* root,
                        Directory** dirs, DWORD* count, bool boot = false)
{
    boot;

    UDFImage* udf_image = &desc->UDFImage;
    
    for(int i = 0; i < desc->UDFImage.file_set_desc_num; i++)
    {
        FileSetDescriptorEx* fset = desc->UDFImage.FileSetDescriptorEx[i];
        LogicalVolumeDescriptor* logic = fset->LogicalVolumeDescriptor;

        char* buffer = (char*)malloc(logic->LogicalBlockSize);
        if(!buffer)
            return false;

        PtrHolder __holder(buffer);

        long_ad ad = fset->FileSetDescriptor.RootDirectoryICB;
        if(ReadUDFBlock(image, udf_image, logic, ad, buffer) == ad.extLength)
        {
            for(int index = 0; index < (int)ad.extLength;)
            {
                tag* tg = (tag*)(buffer + index);
                switch(tg->TagIdentifier)
                {
                case TAG_IDENT_FID:
                    {
                        FileIdentifierDescriptor* fid = (FileIdentifierDescriptor*)tg;
                        index += (sizeof(*fid) + fid->LengthOfFileIdentifier + 1) & ~2;
                        break;
                    }
                case TAG_IDENT_FE:
                    {
                        FileEntry* fentry = (FileEntry*)tg;
                        LoadUDFDir(image, udf_image, logic, fentry, path, root, dirs, count);
                        index += sizeof(*fentry) + fentry->LengthOfAllocationDescriptors + fentry->LengthOfExtendedAttributes;
                        break;
                    }
                default:
                    index = index & ~(0x800 - 1);
                    index += 0x800;
                    break;
                }
            }
        }
    }

    return false;
}

static bool LoadTree( IsoImage* image, PrimaryVolumeDescriptorEx* desc, const char* path,
                      const wchar_t* wpath, DirectoryRecord* root,
                      Directory** dirs, DWORD* count, bool boot = false )
{
    assert( image );
    assert(root);
    assert(count);
    assert(desc);

    if(desc->XBOX)
        return LoadXBOXTree(image, desc, path, desc->XBOXVolumeDescriptor.LocationOfExtent,
                            desc->XBOXVolumeDescriptor.DataLength, dirs, count);
    if(desc->UDF)
        return LoadUDFTree(image, desc, path, root, dirs, count);


    //DebugString( "LoadTree" );
    DWORD block = (DWORD)root->LocationOfExtent;
    DWORD size = (DWORD)root->DataLength;
    
    if( !block || !count)
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
    while( size && offset < size - 0x21 )
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

        if(dirs)
        { // if dirs == 0 - we just calculating total number of records...
            if( desc->Unicode && record->LengthOfFileIdentifier > 1 && num > 1 )
            {
                UINT codepage = CP_UTF8;
                int i;
                int length = record->LengthOfFileIdentifier;
            
                if(!is_utf8(record->FileIdentifier, record->LengthOfFileIdentifier))
                {
                    codepage = GetACP();
                    length = record->LengthOfFileIdentifier / 2;

                    assert(length);

                    WCHAR* uname = (WCHAR*)record->FileIdentifier;
                    for( i = 0; i < length; i++ ) // swap bytes in name
                        uname[i] = (WORD)((uname[i] >> 8) + (uname[i] << 8));

					directory.FileName = (char*)malloc((length + 1) * sizeof(*directory.FileName));
					directory.wFileName = (wchar_t*)malloc((length + 1) * sizeof(*directory.wFileName));
					assert(directory.FileName);
					assert(directory.wFileName);
                    if(!directory.FileName || !directory.wFileName)
                    {
                        free(data);
                        return false;
                    }

                    for(int __i = 0; __i < length; __i++)
                        directory.wFileName[__i] = uname[__i];
                    WideCharToMultiByte( codepage, 0, (WCHAR*)record->FileIdentifier, length,
                                         directory.FileName, length, 0, 0 );
                    directory.FileName[length] = 0;
                    directory.wFileName[length] = 0;
                }
                else
			    { // hmmm... it seems here is Joliet extension where UTF-8 codepage used :)
				    directory.FileName = (char*)malloc((record->LengthOfFileIdentifier + 2) * sizeof(*directory.FileName));
				    assert(directory.FileName);
                    if(!directory.FileName)
                    {
                        free(data);
                        return false;
                    }
                    lstrcpynA(directory.FileName, (char*)record->FileIdentifier, record->LengthOfFileIdentifier + 1);
                    directory.FileName[record->LengthOfFileIdentifier] = 0;

                    // convert from UTF-8 to widechar
                    directory.wFileName = (wchar_t*)malloc((length + 1) * sizeof(*directory.wFileName));
                    if(!directory.wFileName)
                    {
                        free(data);
                        return false;
                    }
                    MultiByteToWideChar(codepage, 0, directory.FileName, -1, directory.wFileName, length + 1);
			    }
            
                if( lstrrchr( directory.FileName, ';' ) )
                    *lstrrchr( directory.FileName, ';' ) = 0;

                if( lwcsrchr( directory.wFileName, ';' ) )
                    *lwcsrchr( directory.wFileName, ';' ) = 0;

			    int dirlen = lstrlenA(path) + 1 + lstrlenA(directory.FileName) + 1;
			    directory.FilePath = (char*)malloc(dirlen * sizeof(*directory.FilePath));
			    assert(directory.FilePath);
                if(!directory.FilePath)
                {
                    free(data);
                    return false;
                }
                lstrcatA( lstrcatA( lstrcpyA( directory.FilePath, path ), (path[0]) ? "\\" : "" ), directory.FileName );

                if(wpath)
                {
                    int wdirlen = lstrlenW(wpath) + 1 + lstrlenW(directory.wFileName) + 1;
                    directory.wFilePath = (wchar_t*)malloc(wdirlen * sizeof(*directory.wFilePath));
                    assert(directory.wFilePath);
                    if(!directory.wFilePath)
                    {
                        free(data);
                        return false;
                    }
                    lstrcatW( lstrcatW( lstrcpyW( directory.wFilePath, wpath ), (wpath[0]) ? L"\\" : L"" ), directory.wFileName );
                }

                (*dirs)[*count] = directory;
            }
            else if( !desc->Unicode && record->LengthOfFileIdentifier > 0 && num > 1 )
            {
			    directory.FileName = (char*)malloc((record->LengthOfFileIdentifier + 2) * sizeof(*directory.FileName));
			    assert(directory.FileName);
                if(!directory.FileName)
                {
                    free(data);
                    return false;
                }
                lstrcpynA( directory.FileName, (char*)record->FileIdentifier, record->LengthOfFileIdentifier + 1);
                directory.FileName[record->LengthOfFileIdentifier] = 0;

                if( lstrrchr( directory.FileName, ';' ) )
                    *lstrrchr( directory.FileName, ';' ) = 0;

			    int dirlen = lstrlenA(path) + 1 + lstrlenA(directory.FileName) + 1;
			    directory.FilePath = (char*)malloc(dirlen * sizeof(*directory.FilePath));
			    assert(directory.FilePath);
                if(!directory.FilePath)
                {
                    free(data);
                    return false;
                }
                lstrcatA( lstrcatA( lstrcpyA( directory.FilePath, path ), path[0] ? "\\" : "" ), directory.FileName );

                (*dirs)[*count] = directory;
            }
        }

        if((!desc->Unicode && record->LengthOfFileIdentifier > 0 ||
             desc->Unicode && record->LengthOfFileIdentifier > 1) && num > 1)
        {
            (*count)++;
            if( directory.Record.FileFlags & FATTR_DIRECTORY )
                if( !LoadTree( image, desc, directory.FilePath, directory.wFilePath, &directory.Record, dirs, count ) )
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

            const char* boot_images = "boot.images";
            int len = lstrlenA(path) + 1 + lstrlenA(boot_images) + 1;
            dir->FileName = (char*)malloc(len);
            dir->FilePath = (char*)malloc(len);

            if(!dir->FileName || !dir->FilePath)
            {
                free(data);
                return false;
            }

            if( lstrlenA( path ) )
            {
                lstrcpyA( dir->FilePath, lstrcpyA( dir->FileName, path ) );
                lstrcatA( dir->FilePath, "\\" );
                lstrcatA( dir->FileName, "\\" );
            }
            
            lstrcatA( dir->FilePath, boot_images );
            lstrcatA( dir->FileName, boot_images );
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
            const char* _1_20_ = "_1.20.";
            const char* _1_44_ = "_1.44.";
            const char* _2_88_ = "_2.88.";
            //const char* img = ".img";

            switch( BootEntry->Entry.BootMediaType )
            {
            case 0:
                dir->FileName = (char*)malloc(lstrlenA(no_emul) + lstrlenA(img) + lstrlenA(number) + 1);
                assert(dir->FileName);
                lstrcatA( lstrcatA( lstrcpyA( dir->FileName, no_emul ), img ), number);
                dir->Record.DataLength = BootEntry->Entry.SectorCount * 0x200;
                break;
            case 1:
                dir->FileName = (char*)malloc(lstrlenA(floppy) + lstrlenA(_1_20_) + lstrlenA(number) +
                                              lstrlenA(ima) + 1);
                assert(dir->FileName);
                lstrcatA( lstrcatA( lstrcatA( lstrcpyA( dir->FileName, floppy ), _1_20_), number ), ima );
                dir->Record.DataLength = 0x50 * 0x2 * 0x0f * 0x200;
                break;
            case 2:
                dir->FileName = (char*)malloc(lstrlenA(floppy) + lstrlenA(_1_44_) + lstrlenA(number) +
                                              lstrlenA(ima) + 1);
                assert(dir->FileName);
                lstrcatA( lstrcatA( lstrcatA( lstrcpyA( dir->FileName, floppy ), _1_44_), number ), ima );
                dir->Record.DataLength = 0x50 * 0x2 * 0x12 * 0x200;
                break;
            case 3:
                dir->FileName = (char*)malloc(lstrlenA(floppy) + lstrlenA(_2_88_) + lstrlenA(number) +
                                              lstrlenA(ima) + 1);
                assert(dir->FileName);
                lstrcatA( lstrcatA( lstrcatA( lstrcpyA( dir->FileName, floppy ), _2_88_), number ), ima );
                dir->Record.DataLength = 0x50 * 0x2 * 0x24 * 0x200;
                break;
            case 4:
                {
                    dir->FileName = (char*)malloc(lstrlenA(harddisk) + lstrlenA(img) + lstrlenA(number) + 1);
                    assert(dir->FileName);
                    lstrcatA( lstrcatA( lstrcpyA( dir->FileName, harddisk ), img ), number );
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
                lstrcpyA( mediaType, ".unknown" );
                dir->Record.DataLength = BootEntry->Entry.SectorCount * 0x200;
                break;
            }
            
            int dirlen = lstrlenA( path ) + lstrlenA("\\boot.images\\") + lstrlenA(dir->FileName) + 1;
            dir->FilePath = (char*)malloc(dirlen);
            
            if( lstrlenA( path ) )
                lstrcatA( lstrcatA( lstrcpyA( dir->FilePath, path ), "\\boot.images\\" ), dir->FileName );
            else
                lstrcatA( lstrcpyA( dir->FilePath, "boot.images\\" ), dir->FileName );

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
        return LoadTree( image, image->VolumeDescriptors, "", L"",
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
            wchar_t wsession[50] = L"";
            if(!image->VolumeDescriptors[i].XBOX)
            {
                lstrcpyA(session, "session");
                lstrcatA( session, litoa( i + 1, digs ) );

                lstrcpyW(wsession, L"session");
                lstrcatW( wsession, litow( i + 1, digs ) );
            }
            //else
            //    lstrcpy(session, "XBOX");
            result |= LoadTree( image, image->VolumeDescriptors + i, session, wsession,
                                &image->VolumeDescriptors[i].VolumeDescriptor.DirectoryRecordForRootDirectory,
                                dirs, count, boot );
        }
        return result;
    }
}

static tChangeVolProc   ChangeVolProc;
static tProcessDataProc ProcessDataProc;

static bool ExtractFile( IsoImage* image, Directory* directory, const char* path, const wchar_t* wpath = 0 )
{
    assert( image && directory && (path || wpath));

    bool xbox = directory->VolumeDescriptor->XBOX;
    
    DWORD size = xbox ? directory->XBOXRecord.DataLength : (DWORD)directory->Record.DataLength;
    DWORD block = xbox ? directory->XBOXRecord.LocationOfExtent : (DWORD)directory->Record.LocationOfExtent;
    DWORD sector = xbox ? 0x800 : (WORD)directory->VolumeDescriptor->VolumeDescriptor.LogicalBlockSize;
    DWORD block_increment = 1;

    if( sector == image->RealBlockSize ) // if logical block size == real block size then read/write by 10 blocks
    {
        sector *= 10;
        block_increment = 10;
    }

    char* buffer = (char*)malloc( sector );
    if( !buffer )
    {
        assert( "can't allocate memory for file extract" );
        return false;
    }

    HANDLE file = 0;
    
    for( ; (int)size >= 0; size -= sector, block += block_increment )
    {
        DWORD cur_size = min( sector, size );
        if( cur_size && ReadBlock( image, block, cur_size, buffer ) != cur_size )
        {
            DebugString( "can't read data" );
            if( file )
                CloseHandle( file );
            free( buffer );
            return false;
        }

        if( !file )
        {
            if(path)
                file = CreateFileA( path, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_ARCHIVE, 0 );
            else if(wpath)
                file = CreateFileW( wpath, GENERIC_WRITE, FILE_SHARE_READ, 0, CREATE_ALWAYS,
                                   FILE_ATTRIBUTE_ARCHIVE, 0 );

            if( !file || file == INVALID_HANDLE_VALUE )
            {
                DebugString( "can't create file" );
                return false;
            }
        }

        if( cur_size )
        {
            DWORD write;
            WriteFile( file, buffer, cur_size, &write, 0 );
            if( write != min( sector, size ) )
            {
                DebugString( "can't write file" );
                if( file )
                    CloseHandle( file );
                free( buffer );
                return false;
            }
        }

        if( !ProcessDataProc( image->DirectoryList[image->Index].FileName, // check for user pressed "Cancel"
                              (int)cur_size ) )
        {
            CloseHandle( file );
            DebugString( "Canceled by user" );
            return false;
        }
    }

    if( file )
    {
        if(!xbox)
        {
            FILETIME   dtime;
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
            LocalFileTimeToFileTime( &ftime, &dtime );
            //dtime = ftime;
            SetFileTime( file, &dtime, &dtime, &dtime );
        }
        CloseHandle( file );
    }
    free( buffer );

    return true;
}

HANDLE __stdcall ISO_OpenArchive( tOpenArchiveData* ArchiveData )
{
    DebugString( "OpenArchive" );

	// TrePe - copied from DllMain
	heap = HeapCreate( 0, 1024 * 1024, 0 );
	char buffer[256];
	if(GetEnvironmentVariableA("ISO_WCX_ENABLE_UDF", buffer, sizeof(buffer)))
		ProcessUDF = true;

	if (!SetFilePointerEx_f) { // TrePe - added this if
		HMODULE h = GetModuleHandleA("kernel32.dll");
		if(h)
		{
			SetFilePointerEx_f = (SetFilePointerEx_t*)GetProcAddress(h, "SetFilePointerEx");
			if(!SetFilePointerEx_f)
			{
				DebugString("can't locate SetFilePointerEx function in kernel32.dll. "
					"using old stype file positioning (may fail on large images)");
			}
		}
		else
		{
			DebugString("no kernel32.dll... Linux forever? :)");
		}
	}
	// TrePe - end

	IsoImage* image = GetImage( ArchiveData->ArcName );
    return image;
}

/* commented out by TrePe
HANDLE __stdcall OpenArchiveW( tOpenArchiveDataW* ArchiveData )
{
    DebugString( "OpenArchiveW" );
    IsoImage* image = GetImage( 0, ArchiveData->ArcName );
    return image;
}
*/

template <class hd>
static int tReadHeader( HANDLE hArcData, hd* HeaderData, Directory** dir = 0 )
{
    //DebugString( "tReadHeader" );

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
    if(dir)
        *dir = directory;

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
    }
    else
    {
        HeaderData->FileAttr = FA_ARCHIVE;
        HeaderData->FileAttr |= (directory->XBOXRecord.FileFlags & XBOX_DIRECTORY) ? FA_DIRECTORY : 0;
        HeaderData->FileAttr |= (directory->XBOXRecord.FileFlags & XBOX_HIDDEN) ? FA_HIDDEN : 0;
        HeaderData->PackSize = HeaderData->UnpSize = (int)directory->XBOXRecord.DataLength;
    }

    return 0;
}

/* commented out by TrePe
int __stdcall ReadHeader( HANDLE hArcData, tHeaderData* HeaderData )
{
    Directory* directory;

    int res;
    if((res = tReadHeader(hArcData, HeaderData, &directory)) == 0)
    {
        lstrcpynA( HeaderData->FileName, directory->FilePath, sizeof(HeaderData->FileName) );
        HeaderData->FileName[sizeof(HeaderData->FileName) - 1] = 0;
    }
    return res;
}
*/

// modified by TrePe
int __stdcall ISO_ReadHeaderEx( HANDLE hArcData, tHeaderDataEx* HeaderData )
{
    Directory* directory;

    int res;
    if((res = tReadHeader(hArcData, HeaderData, &directory)) == 0)
    {
        lstrcpynA( HeaderData->FileName, directory->FilePath, sizeof(HeaderData->FileName) );
        HeaderData->FileName[sizeof(HeaderData->FileName) - 1] = 0;
        HeaderData->PackSizeHigh = HeaderData->UnpSizeHigh = heur_high_word(directory->Record.DataLength);
    }
    return res;
}
/* commented out by TrePe
int __stdcall ReadHeaderExW( HANDLE hArcData, tHeaderDataExW* HeaderData )
{
    Directory* directory;

    int res;
    if((res = tReadHeader(hArcData, HeaderData, &directory)) == 0)
    {
        if(directory->wFilePath)
        {
            lstrcpynW( HeaderData->FileName, directory->wFilePath, sizeofa(HeaderData->FileName) );
            HeaderData->FileName[sizeofa(HeaderData->FileName) - 1] = 0;
        }
        else if(directory->FilePath)
        {
            MultiByteToWideChar(GetACP(), 0, directory->FilePath, -1, HeaderData->FileName, sizeofa(HeaderData->FileName));
            HeaderData->FileName[sizeofa(HeaderData->FileName) - 1] = 0;
        }
        HeaderData->PackSizeHigh = HeaderData->UnpSizeHigh = heur_high_word(directory->Record.DataLength);
    }
    return res;
}
*/

// modified by TrePe
int __stdcall ISO_ProcessFile( HANDLE hArcData)
{
    //DebugString( "ProcessFile" );
    if( !hArcData )
        return E_BAD_DATA;

    IsoImage* image = (IsoImage*)hArcData;

    bool result = true;
	/* commented out by TrePe
    if( Operation == PK_EXTRACT )
    {
        DebugString( "Extracting" );
        char path[MAX_PATH * 2];
        path[0] = 0;
        if( DestPath )
            lstrcpyA( path, DestPath );

        if( DestName )
            lstrcatA( lstrcatA( path, path[0] ? "\\" : "" ), DestName );

        result = ExtractFile( image, &image->DirectoryList[image->Index], path );
    }
	*/
    image->Index++;
    return result ? 0 : E_EWRITE;
}

/* commented out by TrePe
int __stdcall ProcessFileW( HANDLE hArcData, int Operation, wchar_t* DestPath, wchar_t* DestName )
{
    //DebugString( "ProcessFile" );
    if( !hArcData )
        return E_BAD_DATA;

    IsoImage* image = (IsoImage*)hArcData;

    bool result = true;
    if( Operation == PK_EXTRACT )
    {
        DebugString( "Extracting" );
        wchar_t path[MAX_PATH * 2];
        path[0] = 0;
        if( DestPath )
            lstrcpyW( path, DestPath );

        if( DestName )
            lstrcatW( lstrcatW( path, path[0] ? L"\\" : L"" ), DestName );

        result = ExtractFile( image, &image->DirectoryList[image->Index], 0, path );
    }

    image->Index++;
    return result ? 0 : E_EWRITE;
}
*/

// modified by TrePe
int __stdcall ISO_CloseArchive( HANDLE hArcData )
{
    DebugString( "CloseArchive" );
    FreeImage( (IsoImage*)hArcData );

	// TrePe - copied from DllMain
	if( heap ) {
		HeapDestroy( heap );
		heap = 0;
	}

	return 0;
}

/* commented out by TrePe
void __stdcall SetChangeVolProc( HANDLE, tChangeVolProc pChangeVolProc )
{
    ChangeVolProc = pChangeVolProc;
}

void __stdcall SetProcessDataProc( HANDLE, tProcessDataProc pProcessDataProc )
{
    ProcessDataProc = pProcessDataProc;
}

BOOL __stdcall CanYouHandleThisFile( char* FileName )
{
    if( !FileName )
        return FALSE;

    IsoImage* image = GetImage( FileName );

    if( !image )
        return FALSE;
    else
    {
        FreeImage( image );
        return TRUE;
    }
}

BOOL __stdcall CanYouHandleThisFileW( wchar_t* FileName )
{
    if( !FileName )
        return FALSE;

    IsoImage* image = GetImage( 0, FileName );

    if( !image )
        return FALSE;
    else
    {
        FreeImage( image );
        return TRUE;
    }
}

int __stdcall GetPackerCaps()
{
    return PK_CAPS_BY_CONTENT | PK_CAPS_SEARCHTEXT;
}
*/


