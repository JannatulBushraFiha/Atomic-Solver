#ifndef DIMENSIONS_H
#define DIMENSIONS_H

class Dimensions {
public:
    int width;
    int length;
    int depth;

    Dimensions(int w = 0, int l = 0, int dp = 0) {
        width = w;
        length = l;
        depth = dp;
    }

    bool compare(const Dimensions& other) const {
        // checks if this fits inside other
        if (width > other.width) return false;
        if (length > other.length) return false;
        if (depth > other.depth) return false;
        return true;
    }

    long long volume() const {
        return (long long)width * length * depth;
    }
};

#endif
