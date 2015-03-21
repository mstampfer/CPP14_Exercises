#include <map>
#include <string>
#include <iostream>
#include <math.h>
#include <vector>
#include <set>
#include <algorithm>
#include <functional>

#include "pretty_printer.h"
#include <boost/type_index.hpp>

template <typename T>
class TD;


using namespace std;
using boost::typeindex::type_id_with_cvr;

using PreferenceT =  pair<const string, const double>;
using PreferencePairT = pair<const string, PreferenceT>;
using PreferenceMMapT = multimap<const string, PreferenceT>;
using CommonPrefMapT = multimap<string, pair<double, double>>;
using SimilarityPrefMapT = multimap<const string, pair<double, string>>;

auto find_common(const PreferenceMMapT &prefs, const string &person1, const string &person2) {
    CommonPrefMapT common_items;
    const auto p1_items = prefs.equal_range(person1);
    const auto p2_items = prefs.equal_range(person2);
    vector<PreferencePairT> person1v(p1_items.first,p1_items.second);
    vector<PreferencePairT> person2v(p2_items.first,p2_items.second);

    for(auto e1 : person1v) {
        for(auto e2 : person2v) {
            auto e1pair = e1.second; auto e2pair = e2.second;
            if (e1pair.first == e2pair.first) {
                auto p = make_pair(e1pair.second, e2pair.second);
                common_items.insert(pair<string, decltype(p)>(e1.first, p));
            }
        }
    }
    return common_items;
}

auto sim_distance(const PreferenceMMapT &prefs,
        const string& person1,
        const string& person2)
{
    auto sum_of_squares = 0.0;
    auto common = find_common(prefs, person1, person2);
    for_each(common.cbegin(),common.cend(),[&sum_of_squares](auto& item)
    {
        sum_of_squares += pow(item.second.first - item.second.second, 2);
    });
    if (sum_of_squares == 0)
        return 0.0;
    return 1.0/(1.0+sum_of_squares);
}


//Returns the Pearson correlation coefficient for p1 and p2
auto sim_pearson(const PreferenceMMapT & prefs, const string& person1, const string& person2) {

// Get the list of mutually rated items
    auto common = find_common(prefs, person1, person2);
    if (common.size() == 0.0)
        return 0.0;

//       Add up all the preferences
    auto sum1=0.0, sum2=0.0;
    for_each(common.cbegin(), common.cend(), [&sum1, &sum2](auto prefMap)
    {
        sum1 += prefMap.second.first;
        sum2 += prefMap.second.second;
    });

//       Sum up the squares
    auto sum1Sq=0.0, sum2Sq=0.0;
    for_each(common.cbegin(), common.cend(), [&sum1Sq, &sum2Sq](auto prefMap)
    {
        sum1Sq += pow(prefMap.second.first,2);
        sum2Sq += pow(prefMap.second.second,2);
    });

//       Sum up the products
    auto pSum = 0.0;
    for_each(common.cbegin(), common.cend(), [&pSum](auto prefMap)
    {
        pSum += prefMap.second.first*prefMap.second.second;
    });

//       Calculate Pearson score
        auto n = common.size();
        auto num=pSum-(sum1*sum2/n);
        auto den=sqrt((sum1Sq-pow(sum1,2)/n)*(sum2Sq-pow(sum2,2)/n));
        if (den==0) return 0.0;
        auto r=num/den;
        return r;
}

// Returns the best matches for person from the prefs dictionary.
// Number of results and similarity function are optional params.
auto topMatches(const PreferenceMMapT & prefs,
        const string &person,
        function<double(
                const PreferenceMMapT,
                const string &person1,
                const string &person2)> similarity = sim_pearson)
{
    map<double, string, greater<double>> scores;
    for_each(prefs.cbegin(), prefs.cend(), [&prefs, &scores, &similarity, &person](auto &pref) {
        if (pref.first != person)
            scores.insert(make_pair(similarity(prefs, pref.first, person), pref.first));
    });
    return scores;
}

auto all_names(const PreferenceMMapT &prefs) {
    set<string> names;
    for (auto &pref : prefs) {
        names.insert(pref.first);
    }
    return names;
}

auto all_movies(const PreferenceMMapT &prefs, const string person) {
    set<string> movies;
    auto p = prefs.equal_range(person);

    for (auto it = p.first; it != p.second; ++it) {
        movies.insert(it->second.first);
    }
    return movies;
}

// Gets recommendations for a person by using a weighted average
// of every other user's rankings
auto getRecommendations(const PreferenceMMapT &prefs,
        const string &person,
        function<double(
                const PreferenceMMapT,
                const string &person1,
                const string &person2)> similarity = sim_pearson) {
    auto names = all_names(prefs);
    map<string, double> totals;
    map<string, double> simSums;
    auto personprefs = prefs.equal_range(person);
    auto personmovies = all_movies(prefs, person);

    for (auto &name:names) {
        if (name != person) // don't compare me to myself
        {
            auto sim = similarity(prefs, person, name);
            // ignore scores of zero or lower
            if (sim > 0) {

                auto namedprefs = prefs.equal_range(name);
                for (auto it = namedprefs.first; it != namedprefs.second; ++it) {

                    // only score movies I haven't seen yet
                    if (personmovies.find(it->second.first) == personmovies.cend()) {
                        // Similarity * Score
                        totals[it->second.first] += it->second.second * sim;
                        //Sum of similarities
                        simSums[it->second.first] += sim;
                    }

                }
            }

        }
    }
    // Create the normalized list
    map<double, string, greater<double>> rankings;
    for (auto &item : totals) {
        rankings[item.second / simSums[item.first]] = item.first;
    }
    //# Return the inverse sorted list
    return rankings;
}

auto transformPrefs(const PreferenceMMapT &prefs) {
    PreferenceMMapT result;
    for (auto &pref : prefs) {
        result.insert(make_pair(pref.second.first,
                make_pair(pref.first, pref.second.second)));
    }
    return result;
}

auto calculateSimilarItems(const PreferenceMMapT &prefs, int n = 10) {
// Create a dictionary of items showing which other items they are most similar to.
    SimilarityPrefMapT result;
// Invert the preference matrix to be item-centric
    auto itemPrefs = transformPrefs(prefs);
    int c = 0;
    for (auto &item : itemPrefs) {
// Status updates for large datasets
        ++c;
        if (c % 100 == 0.0)
            cout << c << "/" << itemPrefs.size() << endl;
// Find the most similar items to this one
        auto scores = topMatches(itemPrefs, item.first, sim_distance);
        for (auto &score : scores) {
            result.insert(make_pair(item.first, score));
        }
    }
    return result;
}

auto getRecommendedItems(const PreferenceMMapT &prefs,
        const SimilarityPrefMapT &itemMatch,
        const string &user) {
    map<string, double> scores;
    map<string, double> totalSim;

    // Return the rankings from highest to lowest rankings.sort( )
    map<double, string, greater<double>> rankings;

    auto userRatingsMap = prefs.equal_range(user);
    auto usermovies = all_movies(prefs, user);


    // Loop over items rated by this user
    for (auto userRating = userRatingsMap.first; userRating != userRatingsMap.second; ++userRating) {
        auto item = userRating->second.first;
        auto rating = userRating->second.second;

        // Loop over items similar to this one
        auto similarityMap = itemMatch.equal_range(item);
        for (auto similarityRating = similarityMap.first; similarityRating != similarityMap.second; ++similarityRating) {
            auto similarity = similarityRating->second.first;
            auto item2 = similarityRating->second.second;

            // Ignore if this user has already rated this item
            if (usermovies.find(item2) == usermovies.cend()) {

                // Weighted sum of rating times similarity
                scores[item2] += similarity * rating;

                // Sum of all the similarities
                totalSim[item2] += similarity;
            }
        }

        // Divide each total score by total weighting to get an average
        for (auto &score : scores) {
            auto item = score.first;
            auto rank = score.second;
            rankings.insert(make_pair(rank / totalSim[item], item));
        }
    }
    return rankings;
}


int main() {

   const PreferenceMMapT critics = {
           {"Lisa Rose", {"Lady in the Water", 2.5}},
           {"Lisa Rose", {"Snakes on a Plane", 3.5}},
           {"Lisa Rose", {"Just My Luck", 3.0}},
           {"Lisa Rose", {"Superman Returns", 3.5}},
           {"Lisa Rose", {"You, Me and Dupree", 2.5}},
           {"Lisa Rose", {"The Night Listener", 3.0}},
           {"Gene Seymour", {"Lady in the Water", 3.0}},
           {"Gene Seymour", {"Snakes on a Plane", 3.5}},
           {"Gene Seymour", {"Just My Luck", 1.5}},
           {"Gene Seymour", {"Superman Returns", 5.0}},
           {"Gene Seymour", {"The Night Listener", 3.0}},
           {"Gene Seymour", {"You, Me and Dupree", 3.5}},
           {"Michael Phillips", {"Lady in the Water", 2.5}},
           {"Michael Phillips", {"Snakes on a Plane", 3.0}},
           {"Michael Phillips", {"Superman Returns", 3.5}},
           {"Michael Phillips", {"The Night Listener", 4.0}},
           {"Claudia Puig", {"Snakes on a Plane", 3.5}},
           {"Claudia Puig", {"Just My Luck", 3.0}},
           {"Claudia Puig", {"The Night Listener", 4.5}},
           {"Claudia Puig", {"Superman Returns", 4.0}},
           {"Claudia Puig", {"You, Me and Dupree", 2.5}},
           {"Mick LaSalle", {"Lady in the Water", 3.0}},
           {"Mick LaSalle", {"Snakes on a Plane", 4.0}},
           {"Mick LaSalle", {"Just My Luck", 2.0}},
           {"Mick LaSalle", {"Superman Returns", 3.0}},
           {"Mick LaSalle", {"The Night Listener", 3.0}},
           {"Mick LaSalle", {"You, Me and Dupree", 2.0}},
           {"Jack Matthews", {"Lady in the Water", 3.0}},
           {"Jack Matthews", {"Snakes on a Plane", 4.0}},
           {"Jack Matthews", {"The Night Listener", 3.0}},
           {"Jack Matthews", {"Superman Returns", 5.0}},
           {"Jack Matthews", {"You, Me and Dupree", 3.5}},
           {"Toby", {"Snakes on a Plane", 4.5}},
           {"Toby", {"You, Me and Dupree", 1.0}},
           {"Toby", {"Superman Returns", 4.0}}};

//    cout << sim_distance(critics, "Lisa Rose", "Gene Seymour") << endl;
//
//    cout << sim_pearson(critics, "Lisa Rose", "Gene Seymour") << endl;
//
//    auto scores = topMatches(critics,"Toby", sim_distance);
//    for_each(scores.cbegin(), scores.cend(), [](auto& score){
//        cout << score.first << ", " << score.second << endl;
//    });

//    auto rankings = getRecommendations(critics, "Toby", sim_pearson);
//    for_each(rankings.cbegin(), rankings.cend(), [](auto &rank) {
//        cout << rank.first << ", " << rank.second << endl;
//    });

//    auto movies = transformPrefs(critics);
//    auto topMatch = topMatches(movies, "Superman Returns");
//    for_each(topMatch.cbegin(), topMatch.cend(), [](auto &score) {
//        cout << score.first << ", " << score.second << endl;
//    });


    auto itemsIm = calculateSimilarItems(critics, 3);
    for_each(itemsIm.cbegin(), itemsIm.cend(), [](auto &item) {
        cout << item.first << ", " << item.second.first << ", " << item.second.second << endl;
    });

//    auto rankings = getRecommendedItems(critics, itemsIm, "Toby" );
//    for(auto& ranking : rankings)
//    {
//        cout << ranking.first << " " <<  ranking.second << " " << endl;
//    }
    return 0;
}


