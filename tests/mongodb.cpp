#include <iostream>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

int main(int, char**) {
    mongocxx::instance inst{};
    mongocxx::client conn{mongocxx::uri{"mongodb://localhost:27017"}};
    mongocxx::database db = conn["agdb"];
    mongocxx::collection scans = db["scans"];

    auto doc = bsoncxx::builder::basic::document{};
    auto doc_cameras = bsoncxx::builder::basic::array{};
    doc.append(bsoncxx::builder::basic::kvp("scanid","woah!"));
    doc.append(bsoncxx::builder::basic::kvp("start",bsoncxx::types::b_int64{3334443434}));

    scans.insert_one(doc.view());

    mongocxx::stdx::optional<bsoncxx::document::value> maybe_result = scans.find_one(bsoncxx::builder::stream::document{} << bsoncxx::builder::stream::finalize);
    if(maybe_result) {
        std::cout << bsoncxx::to_json(*maybe_result) << '\n';
    }

    return 0;
}
