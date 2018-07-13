#include <iostream>
#include <cassert>

#include <bsoncxx/builder/basic/document.hpp>
#include <bsoncxx/builder/basic/array.hpp>
#include <bsoncxx/builder/basic/kvp.hpp>
#include <bsoncxx/json.hpp>
#include <bsoncxx/types.hpp>

#include <mongocxx/client.hpp>
#include <mongocxx/instance.hpp>

#include "json.hpp"


using namespace std;
using json = nlohmann::json;

using bsoncxx::builder::stream::document;
using bsoncxx::builder::concatenate_doc;
using bsoncxx::builder::stream::open_document;
using bsoncxx::builder::stream::close_document;
using bsoncxx::builder::stream::open_array;
using bsoncxx::builder::stream::close_array;
using bsoncxx::builder::stream::finalize;


/*
 * Example: Get lowest priority that meets the criteria
 *
 */
int main() {
    // New Mongo Connection
    mongocxx::client _conn{mongocxx::uri{ "mongodb://localhost:27017"}};
    mongocxx::database _db = _conn["agdb"];
    mongocxx::collection _pretasks = _db["pretasks"];
    
    // Sort options (ascending)
    auto order = document{} << "priority" << 1 << finalize;
    auto opts = mongocxx::options::find{};
    opts.sort(order.view());
    
    // Building the query
    document query, condition_1, condition_2;
    //condition_1 << "detection" << open_document << "$gte" << 0 << close_document;
    //condition_2 << "preprocess" << open_document << "$gte" << 0 << close_document;
    condition_1 << "detection" << 0;
    condition_2 << "preprocess" << 0;

    query << "$or" << open_array <<
    open_document << concatenate_doc{condition_1.view()} << close_document <<
    open_document << concatenate_doc{condition_2.view()} << close_document << close_array;

    cout << "Query: ";
    std::cout << bsoncxx::to_json(query.view()) << std::endl;
    
    // Result
    bsoncxx::stdx::optional<bsoncxx::document::value> val = _pretasks.find_one(query.view(), opts);
    cout << "Result: ";
    cout << bsoncxx::to_json(*val) << endl;
    
    // To useable json
    json doc = json::parse(bsoncxx::to_json(*val));
    
    // Run detection?
    try {
        assert(doc.at("detection") == 0);
        cout << "---> Run Detection!" << endl;
    } catch (const exception &e) {
        cout << ".... No need to run detection (" << e.what() << ")" << endl;
    }
    
    // Run preprocess?
    try {
        assert(doc.at("preprocess") == 0);
        cout << "---> Run Preprocess!" << endl;
    } catch (const exception &e) {
        cout << ".... No need to run preprocess (" << e.what() << ")" << endl;
    }
    
    return 0;
}

/*
int main() {
    // New Mongo Connection
    mongocxx::client _conn{mongocxx::uri{ "mongodb://localhost:27017"}};
    mongocxx::database _db = _conn["agdb"];
    mongocxx::collection _pretasks = _db["pretasks"];
   
    // Get lowest priority that meets the criteria
    auto order = document{} << "priority" << -1 << finalize;
    auto opts = mongocxx::options::find{};
    opts.sort(order.view());
    bsoncxx::stdx::optional<bsoncxx::document::value> val = _pretasks.find_one({}, opts);

    int priority = json::parse(bsoncxx::to_json(*val))["priority"];
    ++priority;
    
    // Create the document
    bsoncxx::document::value document = document{}  
            << "scanid" << "2018-2323"
            << "priority" << priority
            << "h5filepath" << "file/path/to/file"
            << finalize;
   
    // Insert
    auto ret = _pretasks.insert_one(document.view());
    cout << ret << endl;
}
*/
