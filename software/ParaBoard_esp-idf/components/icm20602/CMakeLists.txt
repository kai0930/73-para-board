idf_component_register(
    SRCS "icm20602.cpp"
    INCLUDE_DIRS "include"
    REQUIRES create_spi config driver
)
