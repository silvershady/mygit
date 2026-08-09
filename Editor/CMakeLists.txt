cmake_minimum_required(VERSION 3.14)
project(arc)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED True)

include(FetchContent)
FetchContent_Declare(ftxui
  GIT_REPOSITORY https://github.com/ArthurSonzogni/FTXUI
  GIT_TAG main
)
FetchContent_MakeAvailable(ftxui)

include_directories(src)

add_executable(arc 
  index_core.cpp 
  src/ui/ThemeUtils.cpp
  src/ui/components/Label.cpp
  src/ui/components/Separator.cpp
  src/ui/components/Panel.cpp
  src/ui/components/StatusBar.cpp
  src/ui/components/ListView.cpp
  src/ui/Renderer.cpp
)
target_link_libraries(arc PRIVATE ftxui::screen ftxui::dom ftxui::component)

install(TARGETS arc RUNTIME DESTINATION bin)

