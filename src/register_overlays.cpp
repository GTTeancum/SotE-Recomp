#include "recomp_overlays.inl"

#include "librecomp/overlays.hpp"

void register_overlays() {
    const recomp::overlays::overlay_section_table_data_t sections{
        .code_sections = section_table,
        .num_code_sections = sizeof(section_table) / sizeof(section_table[0]),
        .total_num_sections = num_sections,
    };
    const recomp::overlays::overlays_by_index_t overlays{
        .table = overlay_sections_by_index,
        .len = sizeof(overlay_sections_by_index) /
               sizeof(overlay_sections_by_index[0]),
    };
    recomp::overlays::register_overlays(sections, overlays);
}
