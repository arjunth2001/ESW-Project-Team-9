#include<bits/stdc++.h>
using namespace std;
typedef long long int lli;
typedef long double ld;
#define vv vector<lli>
#define vov vector<vector<lli>>
#define pp pair<lli, lli>
#define vp vector<pair<lli, lli>>
#define lims(w) (w).begin(), (w).end()
#define mp make_pair
#define f(i, a, b) for (lli i = a; i < b; i++)
#define ss second
#define endl '\n'

vov training_data;
// Configurable
lli DATA_SAMPLES = 200, k = 5;
vector<pair<ld, lli>> distances;
vv current_sample;
map<lli, lli> frequencies;

// Would be replaced with collected data
void store_training_data(){
    srand((unsigned) time(NULL));
    f(i,0,DATA_SAMPLES){
        current_sample.clear();
        lli feature_1 = rand() % 41;
        current_sample.emplace_back(feature_1);
        lli feature_2 = rand() % 11;
        current_sample.emplace_back(feature_2);
        lli feature_3 = rand() % 41;
        current_sample.emplace_back(feature_3);
        lli label = rand() % 3;
        current_sample.emplace_back(label);
        training_data.emplace_back(current_sample);
    }
}

lli knn_predict_count(vv x){
    distances.clear();
    frequencies.clear();
    f(i,0,DATA_SAMPLES){
        lli curr_distance_squared = 0;
        f(j,0,3)
            curr_distance_squared += ((training_data[i][j] - x[j]) * (training_data[i][j] - x[j]));
        distances.emplace_back(mp(sqrt((ld)curr_distance_squared), training_data[i][3]));
        // cout << "Current sample: ";
        // f(j,0,3)
        //     cout << training_data[i][j] << " ";
        // cout << "Label: " << training_data[i][3];
        // cout << endl;
        // cout << "Distance is: " << sqrt((ld)curr_distance_squared) << endl;
    }
    sort(lims(distances));
    f(i,0,k){
        if(frequencies.find(distances[i].ss) == frequencies.end())
            frequencies.insert(mp(distances[i].ss, (lli)1));
        else
            frequencies[distances[i].ss]++;
    }
    auto pr = std::max_element(
        lims(frequencies), [] (const pp & p1, const pp & p2) {
            return p1.second < p2.second;
    });
    return pr->first;
}

int main(){
    store_training_data();
    vv test{ 12, 5, 25 };
    cout << knn_predict_count(test) << endl;
    return 0;
}