add_executable( js_operation_serializer
                main.cpp )
if( UNIX AND NOT APPLE )
  set(rt_library rt )
endif()

target_link_libraries( js_operation_serializer
                       PRIVATE
                       scorum_app
                       graphene_net
                       scorum_chain
                       scorum_protocol
                       scorum_egenesis_none
                       graphene_utilities
                       scorum_wallet
                       fc
                       ${CMAKE_DL_LIBS}
                       ${PLATFORM_SPECIFIC_LIBS} )

install( TARGETS
   js_operation_serializer

   RUNTIME DESTINATION bin
   LIBRARY DESTINATION lib
   ARCHIVE DESTINATION lib
)
