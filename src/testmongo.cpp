#include <iostream>
#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include "json.hpp"

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

using namespace std;
using json = nlohmann::json;
int main() {
    // New Mongo Connection
    mongocxx::client _conn{mongocxx::uri{ "mongodb://localhost:27017"}};
    mongocxx::database _db = _conn["agdb"];
    mongocxx::collection _tasks = _db["tasks"];
   
    // Get highest priority and increment by one
    auto order = bsoncxx::builder::stream::document{} << "priority" << -1 << bsoncxx::builder::stream::finalize;
    auto opts = mongocxx::options::find{};
    opts.sort(order.view());
    bsoncxx::stdx::optional<bsoncxx::document::value> val = _tasks.find_one({}, opts);

    int priority = json::parse(bsoncxx::to_json(*val))["priority"];
    ++priority;
    
    // Create the document
    bsoncxx::document::value document = bsoncxx::builder::stream::document{}  
            << "scanid" << "2018-2323"
            << "priority" << priority
            << "h5filepath" << "file/path/to/file"
            << bsoncxx::builder::stream::finalize;
   
    // Insert
    auto ret = _tasks.insert_one(document.view());
    cout << ret << endl;
}
