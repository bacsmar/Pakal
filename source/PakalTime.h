////////////////////////////////////////////////////////////
//
// SFML - Simple and Fast Multimedia Library
// Copyright (C) 2007-2015 Laurent Gomila (laurent@sfml-dev.org)
//
// This software is provided 'as-is', without any express or implied warranty.
// In no event will the authors be held liable for any damages arising from the use of this software.
//
// Permission is granted to anyone to use this software for any purpose,
// including commercial applications, and to alter it and redistribute it freely,
// subject to the following restrictions:
//
// 1. The origin of this software must not be misrepresented;
//    you must not claim that you wrote the original software.
//    If you use this software in a product, an acknowledgment
//    in the product documentation would be appreciated but is not required.
//
// 2. Altered source versions must be plainly marked as such,
//    and must not be misrepresented as being the original software.
//
// 3. This notice may not be removed or altered from any source distribution.
//
////////////////////////////////////////////////////////////

#pragma once


////////////////////////////////////////////////////////////
// Headers
////////////////////////////////////////////////////////////
#include "config.h"


namespace Pakal
{
////////////////////////////////////////////////////////////
/// \brief Represents a time value
///
////////////////////////////////////////////////////////////
class _PAKALExport Time
{
public:

    ////////////////////////////////////////////////////////////
    /// \brief Default constructor
    ///
    /// Sets the time value to zero.
    ///
    ////////////////////////////////////////////////////////////
	Time() :
		m_microseconds(0)
	{
	}

    ////////////////////////////////////////////////////////////
    /// \brief Return the time value as a number of seconds
    ///
    /// \return Time in seconds
    ///
    /// \see asMilliseconds, asMicroseconds
    ///
    ////////////////////////////////////////////////////////////
	inline float asSeconds() const
	{
		return m_microseconds * 0.000001f;
	}

    ////////////////////////////////////////////////////////////
    /// \brief Return the time value as a number of milliseconds
    ///
    /// \return Time in milliseconds
    ///
    /// \see asSeconds, asMicroseconds
    ///
    ////////////////////////////////////////////////////////////
	inline unsigned asMilliseconds() const
	{
		return static_cast<unsigned>(m_microseconds * 0.001);
	}

    ////////////////////////////////////////////////////////////
    /// \brief Return the time value as a number of microseconds
    ///
    /// \return Time in microseconds
    ///
    /// \see asSeconds, asMilliseconds
    ///
    ////////////////////////////////////////////////////////////
	inline long long asMicroseconds() const
	{
		return m_microseconds;
	}

    ////////////////////////////////////////////////////////////
    // Static member data
    ////////////////////////////////////////////////////////////
    static const Time Zero; ///< Predefined "zero" time value

private:

    friend _PAKALExport Time seconds(float);
    friend _PAKALExport Time milliseconds(unsigned);
    friend _PAKALExport Time microseconds(long long);

    ////////////////////////////////////////////////////////////
    /// \brief Construct from a number of microseconds
    ///
    /// This function is internal. To construct time values,
    /// use sf::seconds, sf::milliseconds or sf::microseconds instead.
    ///
    /// \param microseconds Number of microseconds
    ///
    ////////////////////////////////////////////////////////////
    explicit Time(long long microseconds) :
		m_microseconds(microseconds)
	{
	}

private:

    ////////////////////////////////////////////////////////////
    // Member data
    ////////////////////////////////////////////////////////////
    unsigned long long m_microseconds; ///< Time value stored as microseconds
};

inline Time seconds(float amount)
{
	return Time(static_cast<long long>(amount * 1000000));
}


////////////////////////////////////////////////////////////
inline Time milliseconds(unsigned amount)
{
	return Time(static_cast<long long>(amount) * 1000);
}


////////////////////////////////////////////////////////////
inline Time microseconds(long long amount)
{
	return Time(amount);
}


////////////////////////////////////////////////////////////
inline bool operator ==(Time left, Time right)
{
	return left.asMicroseconds() == right.asMicroseconds();
}


////////////////////////////////////////////////////////////
inline bool operator !=(Time left, Time right)
{
	return left.asMicroseconds() != right.asMicroseconds();
}


////////////////////////////////////////////////////////////
inline bool operator <(Time left, Time right)
{
	return left.asMicroseconds() < right.asMicroseconds();
}


////////////////////////////////////////////////////////////
inline bool operator >(Time left, Time right)
{
	return left.asMicroseconds() > right.asMicroseconds();
}


////////////////////////////////////////////////////////////
inline bool operator <=(Time left, Time right)
{
	return left.asMicroseconds() <= right.asMicroseconds();
}


////////////////////////////////////////////////////////////
inline bool operator >=(Time left, Time right)
{
	return left.asMicroseconds() >= right.asMicroseconds();
}


////////////////////////////////////////////////////////////
inline Time operator -(Time right)
{
	return microseconds(-right.asMicroseconds());
}


////////////////////////////////////////////////////////////
inline Time operator +(Time left, Time right)
{
	return microseconds(left.asMicroseconds() + right.asMicroseconds());
}


////////////////////////////////////////////////////////////
inline Time& operator +=(Time& left, Time right)
{
	return left = left + right;
}


////////////////////////////////////////////////////////////
inline Time operator -(Time left, Time right)
{
	return microseconds(left.asMicroseconds() - right.asMicroseconds());
}


////////////////////////////////////////////////////////////
inline Time& operator -=(Time& left, Time right)
{
	return left = left - right;
}


////////////////////////////////////////////////////////////
inline Time operator *(Time left, float right)
{
	return seconds(left.asSeconds() * right);
}


////////////////////////////////////////////////////////////
inline Time operator *(Time left, long long right)
{
	return microseconds(left.asMicroseconds() * right);
}


////////////////////////////////////////////////////////////
inline Time operator *(float left, Time right)
{
	return right * left;
}


////////////////////////////////////////////////////////////
inline Time operator *(long long left, Time right)
{
	return right * left;
}


////////////////////////////////////////////////////////////
inline Time& operator *=(Time& left, float right)
{
	return left = left * right;
}


////////////////////////////////////////////////////////////
inline Time& operator *=(Time& left, long long right)
{
	return left = left * right;
}


////////////////////////////////////////////////////////////
inline Time operator /(Time left, float right)
{
	return seconds(left.asSeconds() / right);
}


////////////////////////////////////////////////////////////
inline Time operator /(Time left, long long right)
{
	return microseconds(left.asMicroseconds() / right);
}


////////////////////////////////////////////////////////////
inline Time& operator /=(Time& left, float right)
{
	return left = left / right;
}


////////////////////////////////////////////////////////////
inline Time& operator /=(Time& left, long long right)
{
	return left = left / right;
}


////////////////////////////////////////////////////////////
inline float operator /(Time left, Time right)
{
	return left.asSeconds() / right.asSeconds();
}


////////////////////////////////////////////////////////////
inline Time operator %(Time left, Time right)
{
	return microseconds(left.asMicroseconds() % right.asMicroseconds());
}


////////////////////////////////////////////////////////////
inline Time& operator %=(Time& left, Time right)
{
	return left = left % right;
}

} // namespace sf
