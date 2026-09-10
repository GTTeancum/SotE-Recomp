#include "texture_slots.hpp"
#include <iostream>

void require(bool ok) { if (!ok) throw std::runtime_error("texture slot test failed"); }
int main(int argc, char** argv) {
    using namespace sote::texture_slots;
    require(argc == 2);
    const std::filesystem::path path = std::filesystem::path(argv[1]) / "slot_test.json";
    std::filesystem::create_directories(path.parent_path());
    const Key key{3,0x2bc380,32,32,2,1,4,0,0,32768,32,0,0,2,1,1,0x1a4740};
    require(alias(key)==0x30e6c4ff00118fa4ULL); // Python exporter agreement.
    char text[17]; std::snprintf(text, sizeof(text), "%016llx", static_cast<unsigned long long>(alias(key)));
    nlohmann::json valid = {{"version",1},{"slots",nlohmann::json::array({{{"key",key},{"hash",text}}})}};
    auto write = [&](const nlohmann::json& root) { std::ofstream output(path); output << root; };
    Map map;
    write(valid); load(path,map); require(map.size()==1 && map.at(key)==alias(key));
    for (size_t i=0;i<key.size();++i) { auto other=key; ++other[i]; require(alias(other)!=alias(key)); }
    auto reject = [&](nlohmann::json root) {
        write(root); bool rejected=false;
        try { load(path,map); } catch (const std::exception&) { rejected=true; }
        require(rejected && map.size()==1 && map.at(key)==alias(key));
    };
    auto bad=valid; bad["version"]=2; reject(bad);
    bad=valid; bad["slots"][0]["key"][0]=-1; reject(bad);
    bad=valid; bad["slots"][0]["hash"]="0000000000000000"; reject(bad);
    bad=valid; bad["slots"][0]["key"].erase(0); reject(bad);
    std::cout << "Slot key " << text << ": context/layout isolation and atomic manifest rejection passed\n";
}
