idf_component_register(
    SRCS "lps25hb.cpp"
    INCLUDE_DIRS "include"
    REQUIRES create_spi config driver
)
