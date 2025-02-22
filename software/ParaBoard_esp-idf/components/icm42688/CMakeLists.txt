idf_component_register(
    SRCS "icm42688.cpp"
    INCLUDE_DIRS "include"
    REQUIRES create_spi config driver
)
