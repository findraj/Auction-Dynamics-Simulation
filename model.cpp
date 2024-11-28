/**
 * @file auction.cpp
 * @brief Auction simulation with multiple bidders
 * Bidders strategies are: Agent-bidding, Ratchet-bidding, and Sniping
 *
 * @authors Marko Olešák (xolesa00), Ján Findra (xfindr01)
 */

#include <iostream>
#include "simlib.h"

const double NUMBER_OF_ITEMS = 10;                       // Number of auction items
const double NUMBER_OF_BIDDERS = 10;                     // Number of bidders
double currentPrice = 5.0;                               // Current price of the auction
double minimalIncrement() { return currentPrice * 0.05; } // Current increment of the auction
bool firstBidPlaced = false;                             // Flag if the first bid was placed for an item
double SingleItemDuration = 60.0;                        // Duration of a single auction item
double RealPrice = 10.0;                                 // Real price of the item
double ItemEndTime = 0;                                  // End time of the current item

Facility biddingFacility; // Facility for bidding

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
        // Priority = 3;
        while ((currentPrice < this->valuation) && !this->isLeading)
        {
            Wait(Exponential(0.3));
            if (((ItemEndTime - Time) < (SingleItemDuration / 3)))
            {
                if (Time > ItemEndTime)
                {
                    printf("Deleting agent bidder with time: %.2f and item end time: %.2f\n", Time, ItemEndTime);
                    Passivate();
                }

                if (currentPrice + minimalIncrement() < this->valuation)
                {
                    Seize(biddingFacility);
                    currentPrice += minimalIncrement();
                    Wait(Exponential(3));
                    printf("[AGENT] bidder placed a bid. New price: %.2f\n", currentPrice);
                    Release(biddingFacility);
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
        // Priority = 1;
        while (currentPrice < this->valuation && !this->isLeading)
        {
            if (Time > ItemEndTime)
            {
                Passivate();
            }

            // Wait(Exponential(2));
            // Place a bid if it's still within the valuation
            if (currentPrice + minimalIncrement() <= valuation)
            {
                Seize(biddingFacility);
                currentPrice += minimalIncrement();
                firstBidPlaced = true;
                printf("[RATCHET] bidder placed a bid. New price: %.2f\n", currentPrice);
                Wait(Exponential(3));
                Release(biddingFacility);
            }
            else
            {
                break; // Stop bidding if the price exceeds the valuation
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
    double valuation = 0;    // Max amount this bidder is willing to pay
    double snipeDelay = 4.0; // Time before the end to place the bid

    SnipingBidder(double val) : valuation(val) {}

    void Behavior()
    {
        Priority = 2;

        if (Time > ItemEndTime)
        {
            Passivate();
        }

        // Wait until just before the auction ends
        double snipeTime = Time + ItemEndTime - Exponential(snipeDelay);
        if (Time < snipeTime)
        {
            Wait(snipeTime - Time);
        }

        if (Time > ItemEndTime)
        {
            Passivate();
        }

        // Place the bid if it's within the bidder's valuation
        if (currentPrice + minimalIncrement() <= valuation)
        {
            Seize(biddingFacility);
            currentPrice += minimalIncrement();
            firstBidPlaced = true;
            printf("[SNIPER] placed a bid. New price: %.2f\n", currentPrice);
            Wait(Exponential(0.2));
            Release(biddingFacility);
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
        RealPrice = Exponential(1000 * Normal(0.5, 0.1));

        currentPrice = RealPrice * Normal(0.8, 0.2);

        // Reset the current price
        printf("Auction started for item valued at %.2f\n", RealPrice);

        // Create bidders
        (new BidderGenerator)->Activate();

        // If there are no bidders in the first 30 seconds, the item is discarded
        FirstBidTimeout *firstBidTimeout = new FirstBidTimeout(this, 30, &firstBidPlaced);

        // Auction loop
        while (Time < ItemEndTime)
        {
            Passivate(); // Wait for bidders to place bids
        }

        // If a bid was placed, the item is sold
        if (firstBidPlaced)
        {
            isSold = true;
            printf("Item sold at price %.2f\n", currentPrice);
        }
        else
        {
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

            // Wait for the next auction
            Wait(Exponential(10));

            delete item;
        }
    }
};

int main()
{
    Init(0, 1000);
    (new Auction)->Activate();
    Run();
    // Create bidders
}