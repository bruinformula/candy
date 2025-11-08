include(FetchContent)

set(GGML_CUSTOM_RUNTIME_DIR "${CMAKE_BINARY_DIR}/ggml_generated")

FetchContent_Declare(
  ggml
  GIT_REPOSITORY https://github.com/ggerganov/ggml.git
  GIT_TAG        master
)

FetchContent_Populate(ggml)

# Save current value to restore after
set(_saved_runtime_output_dir ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})

# Override the variable (NOT the target property) for ggml config step
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${GGML_CUSTOM_RUNTIME_DIR}" CACHE PATH "Temporary override for ggml" FORCE)

add_subdirectory(${ggml_SOURCE_DIR} ${ggml_BINARY_DIR} EXCLUDE_FROM_ALL)

# Restore original value so rest of your project is unaffected
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${_saved_runtime_output_dir} CACHE PATH "" FORCE)
