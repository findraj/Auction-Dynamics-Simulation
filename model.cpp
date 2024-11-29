/**
 * @file auction.cpp
 * @brief Auction simulation with multiple bidders
 * Bidders strategies are: Agent-bidding, Ratchet-bidding, and Sniping
 *
 * @authors Marko Olešák (xolesa00), Ján Findra (xfindr01)
 */

#include <iostream>
#include <ctime>
#include "simlib.h"

using std::string;

const double NUMBER_OF_ITEMS = 3460;                        // Number of auction items
const double NUMBER_OF_BIDDERS = 10;                      // Number of bidders
double currentPrice = 5.0;                                // Current price of the auction
double minimalIncrement() { return currentPrice * 0.05; } // Current increment of the auction
bool firstBidPlaced = false;                              // Flag if the first bid was placed for an item
double SingleItemDuration = 60.0;                         // Duration of a single auction item
double RealPrice = 10.0;                                  // Real price of the item
double ItemEndTime = 0;                                   // End time of the current item
enum BidderType
{
    AGENT,
    RATCHET,
    SNIPER,
    NONE = -1
};
int lastBidder = NONE; // Last bidder

Facility biddingFacility; // Facility for bidding
Histogram winners("Winners", -1, 1, 4);

// Agent-bidding strategy
// Really quickly bids higher than the current price by minimum increment
// If the current price is higher than the bidder's valuation, the bidder stops bidding
class AgentBidder : public Process
{
public:
    double valuation;
    bool isLeading = false;
    AgentBidder(double val) : valuation(val) {}
    void Behavior()
    {
        while ((currentPrice < this->valuation) && !this->isLeading)
        {
            Wait(Exponential(1.0)); // Increased waiting time
            if (((ItemEndTime - Time) < (SingleItemDuration / 3)))
            {
                if (Time > ItemEndTime)
                {
                    printf("Deleting agent bidder with time: %.2f and item end time: %.2f\n", Time, ItemEndTime);
                    Passivate();
                }

                if (currentPrice + minimalIncrement() < this->valuation)
                {
                    Wait(Exponential(0.2)); // Reaction time - or network latency in this case
                    if (Random() < 0.68)
                    {
                        Seize(biddingFacility);
                        // Check whether the price is still under the valuation
                        if (currentPrice + minimalIncrement() < this->valuation)
                        {
                            firstBidPlaced = true;
                            currentPrice += minimalIncrement();
                            Wait(Exponential(1));
                            printf("[AGENT] bidder placed a bid. New price: %.2f\n", currentPrice);
                            lastBidder = AGENT;
                        }
                        Release(biddingFacility);
                    }
                }
                else
                {
                    break;
                }
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
    double valuation = 0;
    bool isLeading = false;

public:
    RatchetBidder(double val) : valuation(val) {}

    void Behavior()
    {
        while (currentPrice < this->valuation && !this->isLeading)
        {
            if (Time > ItemEndTime)
            {
                Passivate();
            }

            Wait(Exponential(3)); // Reaction time
            if (currentPrice + minimalIncrement() <= valuation)
            {
                Wait(Exponential(3)); // Reaction time
                if (Random() < 0.26)
                {
                    Seize(biddingFacility);
                    if (currentPrice + minimalIncrement() <= valuation)
                    {
                        firstBidPlaced = true;
                        currentPrice += minimalIncrement();
                        printf("[RATCHET] bidder placed a bid. New price: %.2f\n", currentPrice);
                        Wait(Exponential(3));
                        lastBidder = RATCHET;
                    }
                    Release(biddingFacility);
                }
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
public:
    double valuation = 0;
    double snipeDelay = 4.0;

    SnipingBidder(double val) : valuation(val) {}

    void Behavior()
    {
        Priority = 2; // TODO

        if (Time > ItemEndTime)
        {
            Passivate();
        }

        double snipeTime = Time + ItemEndTime - Exponential(snipeDelay);
        if (Time < snipeTime)
        {
            Wait(snipeTime - Time);
        }

        if (Time > ItemEndTime)
        {
            Passivate();
        }

        if (currentPrice + minimalIncrement() <= valuation)
        {
            Wait(Exponential(0.5)); // Reaction time
            if (Random() < 0.03)
            {
                Seize(biddingFacility);
                if (currentPrice + minimalIncrement() < this->valuation)
                {
                    currentPrice += minimalIncrement();
                    firstBidPlaced = true;
                    printf("[SNIPER] placed a bid. New price: %.2f\n", currentPrice);
                    Wait(Exponential(0.2));
                    lastBidder = SNIPER;
                }
                Release(biddingFacility);
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
                (new AgentBidder(RealPrice * Normal(1.2, 0.3)))->Activate();
            }
            else if (probability < 0.65)
            {
                (new RatchetBidder(RealPrice * Normal(1.2, 0.3)))->Activate();
            }
            else
            {
                (new SnipingBidder(RealPrice * Normal(1.2, 0.3)))->Activate();
            }
        }
    }
};

class FirstBidTimeout : public Event
{
    Process *id;
    bool *placed;

public:
    FirstBidTimeout(Process *p, double dt, bool *firstBidPlaced) : id(p)
    {
        placed = firstBidPlaced;
        Activate(Time + dt);
    }

    void Behavior()
    {
        if (!*placed)
        {
            printf("No bids were placed in the first 30 seconds, the item is discarded\n");
            id->Cancel();
            winners(NONE);
        }
        Cancel();
    }
};

// Represents one auction item
class AuctionItem : public Process
{
public:
    bool isSold = false;
    void Behavior()
    {
        Priority = 10;
        // Generate bidders
        ItemEndTime = Time + SingleItemDuration;

        // Generate the value of the item
        RealPrice = Exponential(1000 * Normal(1.0, 0.2));
        printf("Created item with value %.2f\n", RealPrice);

        lastBidder = NONE;
        printf("Resetting last bidder to %d\n", lastBidder);

        currentPrice = RealPrice * Normal(0.8, 0.2);

        // Reset the current price
        printf("Auction started for item valued at %.2f\n", RealPrice);

        // Create bidders
        (new BidderGenerator)->Activate();

        // If there are no bidders in the first 30 seconds, the item is discarded
        FirstBidTimeout *firstBidTimeout = new FirstBidTimeout(this, 30, &firstBidPlaced); // TODO: Pomocna fronta miesto boolu

        printf("This auction will end at %.2f\n", ItemEndTime);
        printf("Current time is %.2f\n", Time);
        // Wait until the end of the auction
        Wait(60);
        printf("Auction ended\n");

        // If a bid was placed, the item is sold
        if (firstBidPlaced)
        {
            isSold = true;
            printf("Item sold at price %.2f\n", currentPrice);
            printf("Winner: %d\n", lastBidder);
            winners(lastBidder);
        }
        else
        {
            // Should not happen, it is caught by the timeout
            printf("Item not sold (no bids)\n");
        }
        delete firstBidTimeout;
    }
};

class Auction : public Process
{
public:
    int items_done = 0;
    void Behavior()
    {
        while (items_done < NUMBER_OF_ITEMS)
        {

            printf("Auction started\n");
            // Create an auction item
            // Create and activate an auction item
            AuctionItem *item = new AuctionItem();
            item->Activate();

            // Wait for the auction item to finish
            // while (!item->Idle())
            // {
            //     Passivate();
            // }

            items_done++;
            printf("Items done  %d\n", items_done);

            // Wait for the next auction
            Wait(90);

            // If the item was not deleted by the timeout, cancel it
        }
    }
};

int main()
{
    RandomSeed(time(NULL));
    Init(0, 100 * NUMBER_OF_ITEMS);
    (new Auction)->Activate();
    Run();

    // Statistics
    SetOutput("stats.out");
    printf("Simulation finished\n");
    biddingFacility.Output();
    winners.Output();
}