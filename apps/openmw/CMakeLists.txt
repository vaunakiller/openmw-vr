# local files
set(GAME
    main.cpp
    engine.cpp

    ${CMAKE_SOURCE_DIR}/files/windows/openmw.rc
    ${CMAKE_SOURCE_DIR}/files/windows/openmw.exe.manifest
)

if (ANDROID)
    set(GAME ${GAME} android_main.cpp)
endif()

set(GAME_HEADER
    engine.hpp
)

source_group(game FILES ${GAME} ${GAME_HEADER})

add_openmw_dir (mwrender
    actors objects renderingmanager animation rotatecontroller sky npcanimation vismask
    creatureanimation effectmanager util renderinginterface pathgrid rendermode weaponanimation
    bulletdebugdraw globalmap characterpreview camera viewovershoulder localmap water terrainstorage ripplesimulation
    renderbin actoranimation landmanager navmesh actorspaths recastmesh fogmanager objectpaging
    )

add_openmw_dir (mwinput
    actions actionmanager bindingsmanager controllermanager controlswitch
    inputmanagerimp mousemanager keyboardmanager sdlmappings sensormanager
    )

add_openmw_dir (mwgui
    layout textinput widgets race class birth review windowmanagerimp console dialogue
    windowbase statswindow messagebox journalwindow charactercreation
    mapwindow windowpinnablebase tooltips scrollwindow bookwindow resourceskin
    formatting inventorywindow container hud countdialog tradewindow settingswindow
    confirmationdialog alchemywindow referenceinterface spellwindow mainmenu quickkeysmenu
    itemselection spellbuyingwindow loadingscreen levelupdialog waitdialog spellcreationdialog
    enchantingdialog trainingwindow travelwindow exposedwindow cursor spellicons
    merchantrepair repair soulgemdialog companionwindow bookpage journalviewmodel journalbooks
    itemmodel containeritemmodel inventoryitemmodel sortfilteritemmodel itemview
    tradeitemmodel companionitemmodel pickpocketitemmodel controllers savegamedialog
    recharge mode videowidget backgroundimage itemwidget screenfader debugwindow spellmodel spellview
    draganddrop timeadvancer jailscreen itemchargeview keyboardnavigation textcolours statswatcher
    )

add_openmw_dir (mwdialogue
    dialoguemanagerimp journalimp journalentry quest topic filter selectwrapper hypertextparser keywordsearch scripttest
    )

add_openmw_dir (mwscript
    locals scriptmanagerimp compilercontext interpretercontext cellextensions miscextensions
    guiextensions soundextensions skyextensions statsextensions containerextensions
    aiextensions controlextensions extensions globalscripts ref dialogueextensions
    animationextensions transformationextensions consoleextensions userextensions
    )

add_openmw_dir (mwsound
    soundmanagerimp openal_output ffmpeg_decoder sound sound_buffer sound_decoder sound_output
    loudness movieaudiofactory alext efx efx-presets regionsoundselector watersoundupdater volumesettings
    )

add_openmw_dir (mwworld
    refdata worldimp scene globals class action nullaction actionteleport
    containerstore actiontalk actiontake manualref player cellvisitors failedaction
    cells localscripts customdata inventorystore ptr actionopen actionread actionharvest
    actionequip timestamp actionalchemy cellstore actionapply actioneat
    store esmstore recordcmp fallback actionrepair actionsoulgem livecellref actiondoor
    contentloader esmloader actiontrap cellreflist cellref physicssystem weather projectilemanager
    cellpreloader datetimemanager
    )

add_openmw_dir (mwphysics
    physicssystem trace collisiontype actor convert object heightfield closestnotmerayresultcallback
    contacttestresultcallback deepestnotmecontacttestresultcallback stepper movementsolver
    closestnotmeconvexresultcallback
    )

add_openmw_dir (mwclass
    classes activator creature npc weapon armor potion apparatus book clothing container door
    ingredient creaturelevlist itemlevlist light lockpick misc probe repair static actor bodypart
    )

add_openmw_dir (mwmechanics
    mechanicsmanagerimp stat creaturestats magiceffects movement actorutil
    drawstate spells activespells npcstats aipackage aisequence aipursue alchemy aiwander aitravel aifollow aiavoiddoor aibreathe
    aicast aiescort aiface aiactivate aicombat recharge repair enchanting pathfinding pathgrid security spellcasting spellresistance
    disease pickpocket levelledlist combat steering obstacle autocalcspell difficultyscaling aicombataction actor summoning
    character actors objects aistate trading weaponpriority spellpriority weapontype spellutil tickableeffects
    spellabsorption linkedeffects
    )

add_openmw_dir (mwstate
    statemanagerimp charactermanager character quicksavemanager
    )

add_openmw_dir (mwbase
    environment world scriptmanager dialoguemanager journal soundmanager mechanicsmanager
    inputmanager windowmanager statemanager
    )

# Main executable

set(OPENMW_LINK_TARGETS 
    ${OSG_LIBRARIES}
    ${OSGPARTICLE_LIBRARIES}
    ${OSGUTIL_LIBRARIES}
    ${OSGDB_LIBRARIES}
    ${OSGVIEWER_LIBRARIES}
    ${OSGGA_LIBRARIES}
    ${OSGSHADOW_LIBRARIES}
    ${Boost_SYSTEM_LIBRARY}
    ${Boost_THREAD_LIBRARY}
    ${Boost_FILESYSTEM_LIBRARY}
    ${Boost_PROGRAM_OPTIONS_LIBRARY}
    ${OPENAL_LIBRARY}
    ${FFmpeg_LIBRARIES}
    ${MyGUI_LIBRARIES}
    ${SDL2_LIBRARY}
    ${RecastNavigation_LIBRARIES}
    "osg-ffmpeg-videoplayer"
    "oics"
    components
)

if (USE_SYSTEM_TINYXML)
    set(OPENMW_LINK_TARGETS ${OPENMW_LINK_TARGETS}
        ${TinyXML_LIBRARIES})
endif()
    
if (NOT UNIX)
    set(OPENMW_LINK_TARGETS ${OPENMW_LINK_TARGETS}
        ${SDL2MAIN_LIBRARY})
endif()

if (ANDROID)
    set (OSG_PLUGINS
        -Wl,--whole-archive
    )
    foreach(PLUGIN_NAME ${USED_OSG_PLUGINS})
        set(OSG_PLUGINS ${OSG_PLUGINS} ${OSG_PLUGINS_DIR}/lib${PLUGIN_NAME}.a)
    endforeach()

    set (OSG_PLUGINS
        ${OSG_PLUGINS} -Wl,--no-whole-archive
    )
    set(OPENMW_LINK_TARGETS ${OPENMW_LINK_TARGETS}
        EGL
        android
        log
        dl
        z
        ${OPENSCENEGRAPH_LIBRARIES}
        freetype
        jpeg
        png
    )
endif (ANDROID)

# Fix for not visible pthreads functions for linker with glibc 2.15
if (UNIX AND NOT APPLE)
    set(OPENMW_LINK_TARGETS ${OPENMW_LINK_TARGETS}
        ${CMAKE_THREAD_LIBS_INIT})
endif()

if(APPLE)
    set(BUNDLE_RESOURCES_DIR "${APP_BUNDLE_DIR}/Contents/Resources")

    set(OPENMW_MYGUI_FILES_ROOT ${BUNDLE_RESOURCES_DIR})
    set(OPENMW_SHADERS_ROOT ${BUNDLE_RESOURCES_DIR})

    add_subdirectory(../../files/ ${CMAKE_CURRENT_BINARY_DIR}/files)

    configure_file("${OpenMW_BINARY_DIR}/settings-default.cfg" ${BUNDLE_RESOURCES_DIR} COPYONLY)
    configure_file("${OpenMW_BINARY_DIR}/openmw.cfg" ${BUNDLE_RESOURCES_DIR} COPYONLY)
    configure_file("${OpenMW_BINARY_DIR}/gamecontrollerdb.txt" ${BUNDLE_RESOURCES_DIR} COPYONLY)

    find_library(COCOA_FRAMEWORK Cocoa)
    find_library(IOKIT_FRAMEWORK IOKit)

    set(OPENMW_LINK_TARGETS ${OPENMW_LINK_TARGETS}
        ${COCOA_FRAMEWORK}
        ${IOKIT_FRAMEWORK})

    if (FFmpeg_FOUND)
        find_library(COREVIDEO_FRAMEWORK CoreVideo)
        find_library(VDA_FRAMEWORK VideoDecodeAcceleration)
        set(OPENMW_LINK_TARGETS ${OPENMW_LINK_TARGETS}
            z
            ${COREVIDEO_FRAMEWORK}
            ${VDA_FRAMEWORK})
    endif()
endif(APPLE)

if (BUILD_WITH_CODE_COVERAGE)
    add_definitions (--coverage)
    target_link_libraries(openmw gcov)
    set(OPENMW_LINK_TARGETS ${OPENMW_LINK_TARGETS}
        gcov)
endif()

if (MSVC)
    # Debug version needs increased number of sections beyond 2^16
    if (CMAKE_CL_64)
        set (CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} /bigobj")
    endif (CMAKE_CL_64)
endif (MSVC)
    
if(BUILD_OPENMW)
    if (NOT ANDROID)
        openmw_add_executable(openmw
            ${OPENMW_FILES}
            ${GAME} ${GAME_HEADER}
            ${APPLE_BUNDLE_RESOURCES}
        )
    else ()
        add_library(openmw
            SHARED
            ${OPENMW_FILES}
            ${GAME} ${GAME_HEADER}
        )
    endif ()

    target_link_libraries(openmw ${OPENMW_LINK_TARGETS})
    
    if(APPLE)
        add_custom_command(TARGET openmw
            POST_BUILD
            COMMAND cp "${OpenMW_BINARY_DIR}/resources/version" "${BUNDLE_RESOURCES_DIR}/resources")
    endif(APPLE)

    if (WIN32)
        INSTALL(TARGETS openmw RUNTIME DESTINATION ".")
    endif (WIN32)
endif()

if(BUILD_OPENMW_VR)
# Use of FetchContent to include the OpenXR SDK requires CMake 3.11
    if(${CMAKE_VERSION} VERSION_LESS "3.11") 
        message(FATAL_ERROR "Building openmw_vr requires CMake version 3.11 or later.")
    endif()

# TODO: Openmw and openmw_vr should preferrably share game code as a static or shared library
# instead of being compiled separately, though for now that's not possible as i depend on
# USE_OPENXR preprocessor switches.
    set(OPENMW_VR_FILES
        vrengine.cpp
    )
    add_openmw_dir (mwvr
         openxraction openxractionset openxrdebug openxrinput openxrmanager openxrmanagerimpl openxrswapchain openxrswapchainimpl
         realisticcombat 
         vranimation vrcamera vrenvironment vrgui vrinputmanager vrinput vrsession vrframebuffer vrshadow vrtypes vrview vrviewer
        )

    openmw_add_executable(openmw_vr
        ${OPENMW_FILES}
        ${OPENMW_VR_FILES}
        ${GAME} ${GAME_HEADER}
        ${APPLE_BUNDLE_RESOURCES}
    )
    
    ########### Import the OpenXR SDK
    # Force the openxr-sdk to use its bundled jsoncpp to avoid problems from system jsoncpp if present
    set(BUILD_WITH_SYSTEM_JSONCPP off) 
    include(FetchContent)
    FetchContent_Declare( 
        OpenXR 
        GIT_REPOSITORY https://github.com/KhronosGroup/OpenXR-SDK.git
        GIT_TAG release-1.0.12
    )
    FetchContent_MakeAvailable(OpenXR)

    target_link_libraries(openmw_vr openxr_loader)
    # openxr makes include directories private
    # Delete this line when the issue is solved: https://github.com/KhronosGroup/OpenXR-SDK-Source/issues/195
    target_include_directories(openmw_vr PRIVATE ${openxr_SOURCE_DIR}/include)

    # Preprocessor variable used to control code paths to vr code
    target_compile_options(openmw_vr PUBLIC -DUSE_OPENXR -DXR_USE_GRAPHICS_API_OPENGL -DXR_USE_PLATFORM_WIN32)
    target_link_libraries(openmw_vr ${OPENMW_LINK_TARGETS})
    if(APPLE)
        add_custom_command(TARGET openmw_vr
            POST_BUILD
            COMMAND cp "${OpenMW_BINARY_DIR}/resources/version" "${BUNDLE_RESOURCES_DIR}/resources")
    endif(APPLE)

    if (WIN32)
        INSTALL(TARGETS openmw_vr RUNTIME DESTINATION ".")
    endif (WIN32)
endif()

# Sound stuff - here so CMake doesn't stupidly recompile EVERYTHING
# when we change the backend.
include_directories(
    ${FFmpeg_INCLUDE_DIRS}
)
