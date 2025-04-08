#include <array>
#include <optional>
#include <print>
#include <thread>
#include <vector>
#include <wincpp/process.hpp>

using namespace wincpp;

static void whitelist_thread(
    const std::unique_ptr< process_t >& process,
    std::shared_ptr< core::handle_t > handle,
    std::size_t id,
    std::uintptr_t start_address );

std::int32_t main( )
{
    try
    {
        std::unique_ptr< process_t > process = process_t::open( "RobloxPlayerBeta.exe" );

        DWORD thread_id = 0;

        const auto& message_box_indirect = process->module_factory[ "user32.dll" ][ "MessageBoxIndirectA" ];

        const auto& data_allocation = process->memory_factory.allocate( 0x1000, memory::protection_flags_t::readwrite );

        auto text_address = data_allocation->address( );
        auto caption_address = data_allocation->address( ) + 0x100;
        process->memory_factory.write< std::string >( text_address, "Hello, World!" );
        process->memory_factory.write< std::string >( caption_address, "Greetings" );

        MSGBOXPARAMSA msgboxParams = { .cbSize = sizeof( MSGBOXPARAMSA ),
                                       .hwndOwner = NULL,
                                       .hInstance = reinterpret_cast< HINSTANCE >( process->module_factory.main_module( ).address( ) ),
                                       .lpszText = reinterpret_cast< LPCSTR >( text_address ),
                                       .lpszCaption = reinterpret_cast< LPCSTR >( caption_address ),
                                       .dwStyle = MB_OK | MB_ICONINFORMATION,
                                       .lpszIcon = NULL,
                                       .dwContextHelpId = 0,
                                       .lpfnMsgBoxCallback = NULL,
                                       .dwLanguageId = MAKELANGID( LANG_NEUTRAL, SUBLANG_DEFAULT ) };

        const auto& params_allocation = process->memory_factory.allocate< MSGBOXPARAMSA >( memory::protection_flags_t::readwrite );
        params_allocation->write( 0x0, msgboxParams );

        std::println( "[+] Allocated {} bytes at {:#x}", sizeof( MSGBOXPARAMSA ), params_allocation->address( ) );

        const auto handle = core::handle_t::create( CreateRemoteThreadEx(
            process->handle->native,
            nullptr,
            0,
            reinterpret_cast< LPTHREAD_START_ROUTINE >( message_box_indirect.address( ) ),
            reinterpret_cast< LPVOID >( params_allocation->address( ) ),
            CREATE_SUSPENDED,
            nullptr,
            &thread_id ) );

        if ( handle->native == nullptr )
            throw core::error::from_win32( GetLastError( ) );

        std::println( "[+] Created thread (TID: {}, Handle: {:#x})", thread_id, reinterpret_cast< std::uintptr_t >( handle->native ) );

        whitelist_thread( process, handle, thread_id, message_box_indirect.address( ) );


        // Resume the thread
        if ( ResumeThread( handle->native ) == static_cast< DWORD >( -1 ) )
            throw core::error::from_win32( GetLastError( ) );

        // Wait for the thread to finish
        WaitForSingleObject( handle->native, INFINITE );

        // Get the exit code of the thread
        DWORD exit_code = 0;
        if ( GetExitCodeThread( handle->native, &exit_code ) == 0 )
            throw core::error::from_win32( GetLastError( ) );

        std::println( "[+] Thread exited with code: {:#x}", exit_code );
    }
    catch ( const core::error& e )
    {
        std::println( "Error: {}", e.what( ) );
    }

    return 0;
}

void whitelist_thread(
    const std::unique_ptr< process_t >& process,
    std::shared_ptr< core::handle_t > handle,
    std::size_t id,
    std::uintptr_t start_address )
{
    const auto& hyperion = process->module_factory[ "RobloxPlayerBeta.dll" ];
    const auto& map = process->memory_factory[ hyperion.address( ) + 0x286210 ];

    std::println( "[+] Map address: {:#x}", map.address( ) );

    FILETIME creation_time{ }, exit_time{ }, kernel_time{ }, user_time{ };
    if ( GetThreadTimes( handle->native, &creation_time, &exit_time, &kernel_time, &user_time ) == 0 )
        throw core::error::from_win32( GetLastError( ) );

    const std::uint64_t creation_time_64 = ( static_cast< std::uint64_t >( creation_time.dwHighDateTime ) << 32 ) | creation_time.dwLowDateTime;

    process->thread_factory.suspend_all( );

    memory::pointer_t< std::uintptr_t > root_entry = *map;
    memory::pointer_t< std::uintptr_t > next_entry = *( root_entry + 0x08 );

    auto& result = root_entry;

    if ( ( next_entry + 0x19 ).read< std::uint8_t >( 0 ) == 0 )
    {
        do
        {
            const std::uintptr_t create_time = *( next_entry + 0x28 );

            auto rbp = create_time < creation_time_64;

            if ( create_time == creation_time_64 )
                rbp = static_cast< std::uintptr_t >( *( next_entry + 0x20 ) ) < id;

            auto next_ptr = ( next_entry + 2 * 0x8 );

            if ( !rbp )
            {
                result = next_entry;
                next_ptr = next_entry;
            }

            next_entry = *next_ptr;

        } while ( ( next_entry + 0x19 ).read< std::uint8_t >( 0 ) == 0 );
    }

    if ( ( result + 0x19 ).read< std::uint8_t >( 0 ) )
    {
        const auto& map_entry_allocation = process->memory_factory.allocate( 0x38, memory::protection_flags_t::readwrite, false );
        map_entry_allocation->write( 0x0, root_entry.address( ) );
        map_entry_allocation->write( 0x8, next_entry.address( ) );
        map_entry_allocation->write( 0x10, root_entry.address( ) );
        map_entry_allocation->write( 0x18, 0 );
        map_entry_allocation->write( 0x20, id );
        map_entry_allocation->write( 0x28, creation_time_64 );
        map_entry_allocation->write( 0x30, start_address );

        next_entry.write< std::uintptr_t >( 0x0, map_entry_allocation->address( ) );
        next_entry.write< std::uintptr_t >( 0x8, map_entry_allocation->address( ) );
        next_entry.write< std::uintptr_t >( 0x10, map_entry_allocation->address( ) );
    }

    process->thread_factory.resume_all( );
}
