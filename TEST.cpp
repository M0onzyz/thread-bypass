#include <array>
#include <optional>
#include <print>
#include <thread>
#include <vector>
#include <wincpp/process.hpp>

using namespace wincpp;

static constexpr auto thread_map = 0x286210;

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

        process->thread_factory.suspend_all( );

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

        process->thread_factory.resume_all( );

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
    const auto& map = process->memory_factory[ hyperion.address( ) + thread_map ];

    std::println( "[+] Map address: {:#x}", map.address( ) );

    FILETIME creation_time{ }, exit_time{ }, kernel_time{ }, user_time{ };
    if ( GetThreadTimes( handle->native, &creation_time, &exit_time, &kernel_time, &user_time ) == 0 )
        throw core::error::from_win32( GetLastError( ) );

    const std::uint64_t creation_time_64 = ( static_cast< std::uint64_t >( creation_time.dwHighDateTime ) << 32 ) | creation_time.dwLowDateTime;

    memory::pointer_t< std::uintptr_t > root_entry = *map;
    const std::size_t map_size = *( map + 0x8 );
    memory::pointer_t< std::uintptr_t > next_entry = *( root_entry + 0x8 );

    auto& result = root_entry;

    // This map is sorted by thread create time. Since we suspend the process before creating the thread, this thread will be the first one in the
    // list, and we don't need to do any searching.

    std::array< std::uintptr_t, 7 > entry{
        root_entry.address( ), next_entry.address( ), root_entry.address( ), 0, id, creation_time_64, start_address
    };

    const auto& map_entry_allocation = process->memory_factory.allocate( 0x38, memory::protection_flags_t::readwrite, false );
    map_entry_allocation->write( 0x0, reinterpret_cast< const std::uint8_t* >( entry.data( ) ), entry.size( ) * sizeof( std::uintptr_t ) );

    *( map + 0x8 ) = map_size + 1;

    std::array< std::uintptr_t, 3 > next_entry_array{ map_entry_allocation->address( ),
                                                      map_entry_allocation->address( ),
                                                      map_entry_allocation->address( ) };

    next_entry.write( 0x0, reinterpret_cast< const std::uint8_t* >( next_entry_array.data( ) ), next_entry_array.size( ) * sizeof( std::uintptr_t ) );
}
