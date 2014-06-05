#include <iostream>
#include "boost/asio.hpp"

#include <queue>

typedef std::pair<unsigned, std::string> queue_value;

struct priority_compare: public std::binary_function<queue_value,
                                                     queue_value, bool >
{
    bool operator ( )( const queue_value& l, const queue_value& r ) const
    {
        return l.first < r.first;
    }
};

int main( )
{

    std::priority_queue< queue_value,
                         std::vector< queue_value >,
                         priority_compare
                       > q;

    q.push( std::make_pair( 1, "test1" ) );
    q.push( std::make_pair( 10, "test10" ) );
    q.push( std::make_pair( -1, "zest-1" ) );
    q.push( std::make_pair( -1, "test-1" ) );
    q.push( std::make_pair( 2, "test2" ) );
    q.push( std::make_pair( 100, "test100" ) );

    std::cout << q.top( ).second << "\n";

    q.pop( );

    return 0;
}

