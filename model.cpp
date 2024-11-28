/**
 * @file auction.cpp
 * @brief Auction simulation with multiple bidders
 * Bidders strategies are: Agent-bidding, Ratchet-bidding, and Sniping
 *
 * @authors Marko Olešák (xolesa00), Ján Findra (xfindr01)
 */

#include "auction.hpp"

const double NUMBER_OF_BIDDERS = 10; // Number of bidders
double minimalIncrement = 2.0;       // Current increment of the auction
double currentPrice = 5.0;           // Current price of the auction

Facility biddingFacility; // Facility for bidding

// Agent-bidding strategy
// Really quickly bids higher than the current price by minimum increment
// If the current price is higher than the bidder's valuation, the bidder stops bidding
class AgentBidder : public Process
{
public:
    double valuation;
    bool isLeading = false;
    void Behavior()
    {
        while (currentPrice < this->valuation)
        {
            if (currentPrice + minimalIncrement < this->valuation)
            {
                currentPrice += minimalIncrement;
            }
            else
            {
                break;
            }
        }
    }
};

// Ratchet-bidding strategy
// Bids higher than the current price by minimum increment
// If the current price is higher than the bidder's valuation, the bidder stops bidding
class RatchetBidder : public Process
{
private:
    double valuation;
    bool isLeading = false;

public:
    void Behavior()
    {
        while (currentPrice < this->valuation)
        {
            if (currentPrice + minimalIncrement < this->valuation)
            {
                currentPrice += minimalIncrement;
            }
            else
            {
                break;
            }
        }
    }
};

// Sniping strategy
// Bids higher than the current price by minimum increment
// If the current price is higher than the bidder's valuation, the bidder stops bidding
// Sniping waits for the last moment to bid
class SnipingBidder : public Process
{
private:
    double valuation;
    bool isLeading = false;

public:
    void Behavior()
    {
        while (currentPrice < this->valuation)
        {
            if (currentPrice + minimalIncrement < this->valuation)
            {
                currentPrice += minimalIncrement;
            }
            else
            {
                break;
            }
        }
    }
};

class BidderGenerator : public Event
{
    void Behavior()
    {
        // Generate bidders
        for (int i = 0; i < NUMBER_OF_BIDDERS; i++)
        {
            // Calculate probability of each strategy
            // Agent-bidding: 40%
            // Ratchet-bidding: 25%
            // Sniping: 35%
            // Follows the reference paper
            double probability = Random();

            // Generate bidder with the given strategy
            if (probability < 0.4)
            {
                (new AgentBidder)->Activate();
            }
            else if (probability < 0.65)
            {
                (new RatchetBidder)->Activate();
            }
            else
            {
                (new SnipingBidder)->Activate();
            }
        }
    }
};

// Represents one auction item
class AuctionItem : public Process
{
public:
    void Behavior()
    {
        // Generate bidders
        (new BidderGenerator)->Activate();

        // If there are no bidders in the first 30 seconds, the item is discarded
        Wait(30); // Wrong
    }
};

class Auction : public Process
{
public:
    int items_sold = 0;
    void Behavior()
    {
        // Create an auction item
        (new AuctionItem)->Activate();

        // Wait for the auction to finish
    }
};

int main()
{
    Init(0, 1000);

    // Create bidders
}