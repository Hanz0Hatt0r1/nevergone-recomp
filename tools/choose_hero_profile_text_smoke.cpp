#include <cassert>
#include <string>

#include "choose_hero_profile_text.h"
#include "login_localization_csv.h"

int main() {
    const std::string csv =
        "keyword,note,CN,CNT,EN,KR,FR,GM,JP,end\n"
        "Hero1Name,,瞳恩,瞳恩,DAWN,아더,DAWN,DAWN,ダウン,NULL\n"
        "Hero2Name,,茜格斯,希格斯,HIGGS,마티나,HIGGS,HIGGS,ヒグス,NULL\n"
        "GdUI08,LV,等级,等級,Lv.,Lv.,Lv.,Lv.,Lv.,NULL\n"
        "GameUSETime,,游戏时间 ,遊戲時間 ,Time ,Time ,Temps ,Zeit ,時間 ,NULL\n";

    nevergone::choose_hero_profile_text::Text text;
    assert(nevergone::choose_hero_profile_text::compose(
        1u,
        17,
        12,
        34,
        csv,
        nevergone::login_localization_csv::kEnglishColumn,
        &text));
    assert(text.name == "DAWN");
    assert(text.level == "Lv.17");
    assert(text.play_time == "Time 12:34");

    assert(nevergone::choose_hero_profile_text::compose(
        2u,
        9,
        3,
        7,
        csv,
        nevergone::login_localization_csv::kSimplifiedChineseColumn,
        &text));
    assert(text.name == "茜格斯");
    assert(text.level == "等级9");
    assert(text.play_time == "游戏时间 03:07");

    assert(nevergone::choose_hero_profile_text::compose(
        2u,
        101,
        0,
        5,
        csv,
        nevergone::login_localization_csv::kJapaneseColumn,
        &text));
    assert(text.name == "ヒグス");
    assert(text.level == "Lv.101");
    assert(text.play_time == "時間 00:05");

    assert(!nevergone::choose_hero_profile_text::compose(
        0u, 1, 0, 0, csv, nevergone::login_localization_csv::kEnglishColumn, &text));
    assert(!nevergone::choose_hero_profile_text::compose(
        1u, 1, 0, 0, "Hero1Name,,,,DAWN\n", nevergone::login_localization_csv::kEnglishColumn, &text));

    return 0;
}
