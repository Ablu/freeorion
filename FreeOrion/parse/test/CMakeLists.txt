cmake_minimum_required(VERSION 2.6)
cmake_policy(VERSION 2.6.4)

project(parser_tests)

message("-- Configuring parser_test")

add_definitions(-DFREEORION_BUILD_SERVER)

add_executable(parser_test
    ../../combat/CombatSystem.cpp
    ../../network/ServerNetworking.cpp
    ../../server/SaveLoad.cpp
    ../../server/ServerApp.cpp
    ../../server/ServerFSM.cpp
    ../../universe/UniverseServer.cpp
    ../../universe/Universe.cpp
    ../../util/AppInterface.cpp
    ../../util/VarText.cpp
    test.cpp
    lexer_test_rules.cpp
)

target_link_libraries(parser_test
    core
    parse
    log4cpp
    ${CMAKE_THREAD_LIBS_INIT}
)

install(
    TARGETS parser_test
    RUNTIME DESTINATION .
    COMPONENT COMPONENT_FREEORION
)

macro (add_test_and_data_files name)
    set(suffix "")
    set(args "")
    foreach (arg ${ARGN})
        if (EXISTS ${CMAKE_SOURCE_DIR}/parse/test/${arg})
            get_filename_component(filename_without_path ${arg} NAME)
            configure_file(${CMAKE_CURRENT_SOURCE_DIR}/${arg}
                           ${CMAKE_CURRENT_BINARY_DIR}/${filename_without_path}
                           COPYONLY)
            set(suffix ${suffix}-${filename_without_path})
            set(args "${args};${filename_without_path}")
        else ()
            set(suffix ${suffix}-${arg})
            set(args "${args};${arg}")
        endif ()
    endforeach ()
    add_test (${name}${suffix} ${CMAKE_BINARY_DIR}/${name} ${args})
endmacro ()

add_test_and_data_files(parser_test lexer -f lexer_tokens)
add_test_and_data_files(parser_test capture_result_enum_parser -f capture_result_enumerators)
add_test_and_data_files(parser_test combat_fighter_type_enum_parser -f combat_fighter_type_enumerators)
add_test_and_data_files(parser_test empire_affiliation_type_enum_parser -f empire_affiliation_type_enumerators)
add_test_and_data_files(parser_test non_ship_part_meter_type_enum_parser -f non_ship_part_meter_type_enumerators)
add_test_and_data_files(parser_test planet_environment_enum_parser -f planet_environment_enumerators)
add_test_and_data_files(parser_test planet_size_enum_parser -f planet_size_enumerators)
add_test_and_data_files(parser_test planet_type_enum_parser -f planet_type_enumerators)
add_test_and_data_files(parser_test ship_part_class_enum_parser -f ship_part_class_enumerators)
add_test_and_data_files(parser_test ship_slot_type_enum_parser -f ship_slot_type_enumerators)
add_test_and_data_files(parser_test star_type_enum_parser -f star_type_enumerators)
add_test_and_data_files(parser_test value_ref_statistic_type_enum_parser -f statistic_type_enumerators)
add_test_and_data_files(parser_test tech_type_enum_parser -f tech_type_enumerators)
add_test_and_data_files(parser_test universe_object_type_enum_parser -f universe_object_type_enumerators)
add_test_and_data_files(parser_test unlockable_item_type_enum_parser -f unlockable_item_type_enumerators)
add_test_and_data_files(parser_test int_value_ref_parser -f int_arithmetic)
add_test_and_data_files(parser_test int_value_ref_parser -f int_variable)
add_test_and_data_files(parser_test int_value_ref_parser -f int_variable_arithmetic)
add_test_and_data_files(parser_test int_value_ref_parser -f int_statistic)
add_test_and_data_files(parser_test double_value_ref_parser -f double_arithmetic)
add_test_and_data_files(parser_test double_value_ref_parser -f double_variable)
add_test_and_data_files(parser_test double_value_ref_parser -f double_variable_arithmetic)
add_test_and_data_files(parser_test double_value_ref_parser -f double_statistic)
add_test_and_data_files(parser_test double_value_ref_parser -f int_arithmetic)
add_test_and_data_files(parser_test double_value_ref_parser -f int_variable)
add_test_and_data_files(parser_test double_value_ref_parser -f int_variable_arithmetic)
add_test_and_data_files(parser_test double_value_ref_parser -f int_statistic)
add_test_and_data_files(parser_test string_value_ref_parser -f string_constant)
add_test_and_data_files(parser_test string_value_ref_parser -f string_variable)
add_test_and_data_files(parser_test string_value_ref_parser -f int_variable)
add_test_and_data_files(parser_test string_value_ref_parser -f int_statistic)
add_test_and_data_files(parser_test string_value_ref_parser -f double_variable)
add_test_and_data_files(parser_test string_value_ref_parser -f double_statistic)
add_test_and_data_files(parser_test planet_size_value_ref_parser -f planet_size_constant)
add_test_and_data_files(parser_test planet_size_value_ref_parser -f planet_size_variable)
add_test_and_data_files(parser_test planet_size_value_ref_parser -f planet_size_statistic)
add_test_and_data_files(parser_test planet_type_value_ref_parser -f planet_type_constant)
add_test_and_data_files(parser_test planet_type_value_ref_parser -f planet_type_variable)
add_test_and_data_files(parser_test planet_type_value_ref_parser -f planet_type_statistic)
add_test_and_data_files(parser_test planet_environment_value_ref_parser -f planet_environment_constant)
add_test_and_data_files(parser_test planet_environment_value_ref_parser -f planet_environment_variable)
add_test_and_data_files(parser_test planet_environment_value_ref_parser -f planet_environment_statistic)
add_test_and_data_files(parser_test universe_object_type_value_ref_parser -f universe_object_type_constant)
add_test_and_data_files(parser_test universe_object_type_value_ref_parser -f universe_object_type_variable)
add_test_and_data_files(parser_test universe_object_type_value_ref_parser -f universe_object_type_statistic)
add_test_and_data_files(parser_test star_type_value_ref_parser -f star_type_constant)
add_test_and_data_files(parser_test star_type_value_ref_parser -f star_type_variable)
add_test_and_data_files(parser_test star_type_value_ref_parser -f star_type_statistic)
add_test_and_data_files(parser_test int_value_ref_evaluation -f int_arithmetic_evaluated)
add_test_and_data_files(parser_test double_value_ref_evaluation -f double_arithmetic_evaluated)
add_test_and_data_files(parser_test condition_parser -f condition_parser_1)
add_test_and_data_files(parser_test condition_parser -f condition_parser_2)
add_test_and_data_files(parser_test condition_parser -f condition_parser_3)
add_test_and_data_files(parser_test effect_parser -f effect_parser)
add_test_and_data_files(parser_test buildings_parser -f buildings)
add_test_and_data_files(parser_test buildings_parser -f buildings_permutations)
add_test_and_data_files(parser_test specials_parser -f specials)
add_test_and_data_files(parser_test specials_parser -f specials_permutations)
add_test_and_data_files(parser_test species_parser -f species)
add_test_and_data_files(parser_test species_parser -f species_permutations)
add_test_and_data_files(parser_test techs_parser -f techs)
add_test_and_data_files(parser_test techs_parser -f techs_permutations)
add_test_and_data_files(parser_test items_parser -f items)
add_test_and_data_files(parser_test items_parser -f items_permutations)
add_test_and_data_files(parser_test ship_parts_parser -f ship_parts)
add_test_and_data_files(parser_test ship_hulls_parser -f ship_hulls)
add_test_and_data_files(parser_test ship_designs_parser -f ship_designs)
add_test_and_data_files(parser_test ship_designs_parser -f ship_designs_permutations)
add_test_and_data_files(parser_test fleet_plans_parser -f fleet_plans)
add_test_and_data_files(parser_test fleet_plans_parser -f fleet_plans_permutations)
add_test_and_data_files(parser_test monster_fleet_plans_parser -f monster_fleet_plans)
add_test_and_data_files(parser_test monster_fleet_plans_parser -f monster_fleet_plans_permutations)

add_test_and_data_files(parser_test int_value_ref_parser -f int_arithmetic_errors --fail)
add_test_and_data_files(parser_test int_value_ref_parser -f int_variable_errors --fail)
add_test_and_data_files(parser_test int_value_ref_parser -f int_statistic_errors --fail)
add_test_and_data_files(parser_test double_value_ref_parser -f double_arithmetic_errors --fail)
add_test_and_data_files(parser_test double_value_ref_parser -f double_variable_errors --fail)
add_test_and_data_files(parser_test double_value_ref_parser -f double_statistic_errors --fail)
add_test_and_data_files(parser_test string_value_ref_parser -f string_constant_errors --fail)
add_test_and_data_files(parser_test string_value_ref_parser -f string_variable_errors --fail)
add_test_and_data_files(parser_test string_value_ref_parser -f string_statistic_errors --fail)
add_test_and_data_files(parser_test planet_size_value_ref_parser -f planet_size_variable_errors --fail)
add_test_and_data_files(parser_test planet_size_value_ref_parser -f planet_size_statistic_errors --fail)
add_test_and_data_files(parser_test planet_type_value_ref_parser -f planet_type_variable_errors --fail)
add_test_and_data_files(parser_test planet_type_value_ref_parser -f planet_type_statistic_errors --fail)
add_test_and_data_files(parser_test planet_environment_value_ref_parser -f planet_environment_variable_errors --fail)
add_test_and_data_files(parser_test planet_environment_value_ref_parser -f planet_environment_statistic_errors --fail)
add_test_and_data_files(parser_test universe_object_type_value_ref_parser -f universe_object_type_variable_errors --fail)
add_test_and_data_files(parser_test universe_object_type_value_ref_parser -f universe_object_type_statistic_errors --fail)
add_test_and_data_files(parser_test star_type_value_ref_parser -f star_type_variable_errors --fail)
add_test_and_data_files(parser_test star_type_value_ref_parser -f star_type_statistic_errors --fail)
add_test_and_data_files(parser_test condition_parser -f condition_parser_1_errors --fail)
add_test_and_data_files(parser_test condition_parser -f condition_parser_2_errors --fail)
add_test_and_data_files(parser_test condition_parser -f condition_parser_3_errors --fail)
add_test_and_data_files(parser_test effect_parser -f effect_parser_errors --fail)
add_test_and_data_files(parser_test buildings_parser -f buildings_errors --fail)
add_test_and_data_files(parser_test specials_parser -f specials_errors --fail)
add_test_and_data_files(parser_test species_parser -f species_errors --fail)
add_test_and_data_files(parser_test techs_parser -f techs_errors --fail)
add_test_and_data_files(parser_test items_parser -f items_errors --fail)
add_test_and_data_files(parser_test ship_parts_parser -f ship_parts_errors --fail)
add_test_and_data_files(parser_test ship_hulls_parser -f ship_hulls_errors --fail)
add_test_and_data_files(parser_test ship_designs_parser -f ship_designs_errors --fail)
add_test_and_data_files(parser_test fleet_plans_parser -f fleet_plans_errors --fail)
add_test_and_data_files(parser_test monster_fleet_plans_parser -f monster_fleet_plans_errors --fail)
