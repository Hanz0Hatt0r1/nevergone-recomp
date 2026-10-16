#include <cassert>
#include <string>

#include "login_localization_csv.h"

int main() {
    const std::string csv =
        "\xef\xbb\xbfkeyword,note,CN,CNT,EN\r\n"
        "Earlier,,x,x,\"first line\nsecond line\"\r\n"
        "FGSub1,,x,x,\"dialogue with \"\"quotes\"\"\"\r\n";
    std::string value;
    assert(nevergone::login_localization_csv::resolve_key(csv, "FGSub1", 4, &value));
    assert(value == "dialogue with \"quotes\"");
    assert(nevergone::login_localization_csv::resolve_key(csv, "Earlier", 4, &value));
    assert(value == "first line\nsecond line");
    assert(!nevergone::login_localization_csv::resolve_key(
        "Broken,,x,x,\"unterminated", "Missing", 4, &value));
}
