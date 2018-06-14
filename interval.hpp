
#ifndef __INTERVAL_H
#define __INTERVAL_H

/* Half-open interval class */
template<class T>
struct Interval
{
	T min_;
	T max_;

	Interval (T t1 = T(0), T t2 = T(0))
	{
		if (t1 < t2) /* ensure min_ <= max_ on construction */
		{
			min_ = t1;
			max_ = t2;
		}
		else
		{
			min_ = t2;
			max_ = t1;
		}
	}

	T min () const { return min_; }
	T max () const { return max_; }
	T width () const { return max_ - min_; }

	/* < and > implement strictly less-than/greater-than.
	 * For example, I1 < I2 if all elements in I1 are less than all elements in I2. */
	bool operator< (const Interval& other) { return max_ < other.min_; }
	bool operator> (const Interval& other) { return min_ > other.max_; }

	/* Two intervals overlap if their intersection has nonzero width */
	bool overlaps (const Interval& other) { return !(*this < other) && !(*this > other); }
};



#endif /* __INTERVAL_H */
