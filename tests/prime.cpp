#include <bits/stdc++.h>

using namespace std;

using ll = long long;
using i128 = __int128_t;

ll witness[] = { 2, 3, 5, 7, 11, 13, 17, 19, 23, 29, 31 };

ll power(ll a, ll b, ll m);
ll check_composite(ll n, ll a, ll d, ll s);
ll miller_rabin(ll n);

int main() {
    ll n;
    cout << "enter: ";
    cin >> n;
    cout << n << " is a " << (miller_rabin(n) ? "prime" : "composite");
    return 0;
}

ll power(ll a, ll b, ll m) {
    ll res = 1;
    while (b) {
        if (b & 1LL) res = (ll)((i128)res * a % m);
        a = (ll)((i128)a * a % m);
        b >>= 1LL;
    }
    return res;
}

ll check_composite(ll n, ll a, ll d, ll s) {
    ll x = power(a, d, n);
    if (x == 1 || x == n - 1) return false;
    for (ll r = 1; r < s; ++ r) {
        x = (ll)((i128)x * x % n);
        if (x == n - 1) return false;
    }
    return true;
}

ll miller_rabin(ll n) {
    if (n < 2) return false;
    if (n <= 3) return true;
    if (n % 2 == 0 || n % 3 == 0) return false;

    ll d = n - 1, s = 0;
    while (!(d & 1LL)) {
        d >>= 1LL;
        ++s;
    }

    for (ll a : witness) {
        if (n == a) return true;
        if (check_composite(n, a, d, s)) return false;
    }

    return true;
}