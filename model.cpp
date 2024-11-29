/**
 * @file auction.cpp
 * @brief Auction simulation with multiple bidders
 * Bidders strategies are: Agent-bidding, Ratchet-bidding, and Sniping
 *
 * @authors Marko Olešák (xolesa00), Ján Findra (xfindr01)
 */

#include <iostream>
#include <ctime>
#include <cmath>
#include "simlib.h"
#include <cstdint>

#define LOGGING true

using std::string;

const double NUMBER_OF_ITEMS = 1000;                      // Number of auction items
const double NUMBER_OF_BIDDERS = 50;                      // Number of bidders
double currentPrice = 5.0;                                // Current price of the auction
double minimalIncrement() { return currentPrice * 0.05; } // Current increment of the auction TODO
bool firstBidPlaced = false;                              // Flag if the first bid was placed for an item
const double SINGLE_ITEM_DURATION = 60.0;                 // Duration of a single auction item
double RealPrice = 10.0;                                  // Real price of the item
double ItemEndTime = 0;                                   // End time of the current item
uint32_t itemNumber = 0;                                  // Statistics
bool roundEnded = false;                                  // Flag if the item has ended

enum BidderType
{
    AGENT,
    RATCHET,
    SNIPER,
    NONE = -1
};
int lastBidder = NONE; // Last bidder

Facility biddingFacility("Bidding process"); // Facility for bidding
Facility runningAuction("Item auction");     // Facility for running the auction
Histogram winners("Winners", -1, 1, 4);      // Histogram of winners

void logSingleBid(double bidAmount)
{
    static bool header = false;
    FILE *logFile = fopen("analysis/results/auction_detailed_log.csv", "a");
    if (logFile)
    {
        if (!header)
        {
            header = true;
            fprintf(logFile, "ItemNumber,ItemTime,BidAmount,Patience\n"); // Header
        }

        double itemTime = SINGLE_ITEM_DURATION - (ItemEndTime - Time);
        fprintf(logFile, "%d,%.1f,%.2f\n", itemNumber, itemTime, bidAmount);
        fclose(logFile);
    }
}
// Agent-bidding strategy
// Really quickly bids higher than the current price by minimum increment
// If the current price is higher than the bidder's valuation, the bidder stops bidding
class AgentBidder : public Process
{
private:
    double valuation;
    bool isLeading = false;
    double patience = 1.0;                                     // Start with patience 1
    double lastUpdateTime = 0;                                 // Last time updatePatience was called
    const double UPDATE_INTERVAL = SINGLE_ITEM_DURATION / 100; // Minimum time interval between updates

public:
    AgentBidder(double val) : valuation(val) {}

    void Behavior()
    {
        while ((currentPrice < this->valuation) && (this->patience > Exponential(0.1))) // Stop if patience runs out
        {
            // Check if enough time has passed since the last update
            if ((Time - lastUpdateTime) >= UPDATE_INTERVAL)
            {
                updatePatience();
                lastUpdateTime = Time; // Update the timestamp
            }

            Wait(std::max(this->patience, 0.2));

            if (Time > ItemEndTime)
            {
                Passivate();
            }

            // Agents do not engage in bidding in the early stages of the auction
            if (Time > (ItemEndTime - (Exponential(SINGLE_ITEM_DURATION / 2))))
            {
                if ((Random() > this->patience) && ((currentPrice + minimalIncrement()) < this->valuation) && !biddingFacility.Busy())
                {
                    Seize(biddingFacility);
                    Wait(Exponential(0.05)); // Network latency
                    if ((currentPrice + minimalIncrement()) < this->valuation)
                    {
                        if (Time > ItemEndTime)
                        {
                            Release(biddingFacility);
                            Passivate();
                        }
                        firstBidPlaced = true;
                        currentPrice += minimalIncrement();
                        if (LOGGING)
                        {
                            logSingleBid(currentPrice);
                        }
                        printf("[AGENT] bidder placed a bid. New price: %.2f\n", currentPrice);
                        lastBidder = AGENT;

                        // Decrease patience slightly with each bid
                        this->patience += Normal(0.05, 0.01);
                    }
                    Release(biddingFacility);
                }
                else
                {
                    // Small patience reduction for waiting without bidding
                    // this->patience -= Normal(0.01, 0.005);
                }
            }
            // Stop if patience is exhausted
            if (this->patience <= 0)
            {
                printf("[AGENT] bidder ran out of patience and stopped bidding.\n");
            }
        }
        Passivate();
    }

    void updatePatience()
    {
        // Normalize time to range [0, 1] over the auction's single item duration
        double normalizedTime = (SINGLE_ITEM_DURATION - (ItemEndTime - Time)) / SINGLE_ITEM_DURATION;

        if (normalizedTime < 0.75)
        {
            this->patience = 1.0 - ((0.01 / 0.75) * normalizedTime);
        }
        else
        {
            // Exponential decline for the remaining 0.2 over [0.75, 1.0]
            double remainingTime = (normalizedTime - 0.75) / (1.0 - 0.75);                      // Normalize [0.9, 1.0] to [0, 1]
            this->patience = 0.99 - 0.2 * pow(remainingTime, 5);                              // Start from 0.98 and drop exponentially
            // printf("Normalized time: %.2f, patience %.2f\n", normalizedTime, this->patience); // TODO: REMOVE
        }

        // TODO: Remove ?
        // if (!this->isLeading)
        // {
        //     this->patience += Normal(0.01, 0.005);
        //     if (this->patience < 0)
        //         this->patience = 0;
        // }
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
    double patience = 1.0;                                     // Start with patience 1
    double lastUpdateTime = 0;                                 // Last time updatePatience was called
    const double UPDATE_INTERVAL = SINGLE_ITEM_DURATION / 100; // Minimum time interval between updates

public:
    RatchetBidder(double val) : valuation(val) {}

    void Behavior()
    {
        while ((currentPrice < this->valuation) && (this->patience > Exponential(0.1)))
        {
            if ((Time - lastUpdateTime) >= UPDATE_INTERVAL)
            {
                updatePatience();
                lastUpdateTime = Time; // Update the timestamp
            }

            Wait(std::max(this->patience, 0.2));

            if (Time > ItemEndTime)
            {
                Passivate();
            }

            // Check if they want to bid
            if ((Random() > this->patience) && ((currentPrice + minimalIncrement()) <= valuation) && !biddingFacility.Busy() && !this->isLeading)
            {
                Seize(biddingFacility);
                Wait(Exponential(0.5)); // Human reaction time + network latency
                if (currentPrice + minimalIncrement() <= valuation)
                {
                    if (Time > ItemEndTime)
                    {
                        Release(biddingFacility);
                        Passivate();
                    }
                    firstBidPlaced = true;
                    currentPrice += minimalIncrement();
                    if (LOGGING)
                    {
                        logSingleBid(currentPrice);
                    }
                    printf("[RATCHET] bidder placed a bid. New price: %.2f\n", currentPrice);
                    lastBidder = RATCHET;
                    this->patience += Normal(0.05, 0.01);
                }
                Release(biddingFacility);
            }
            else
            {
                // Reduce patience slightly when not bidding
                // this->patience -= Normal(0.03, 0.02 / 3);
            }
        }
        if (this->patience <= 0)
            printf("[RATCHET] ran out of patience and stopped bidding.\n");
        Passivate();
    }

    void updatePatience()
    {
        // Normalize time to range [0, 1] over the auction's single item duration
        double normalizedTime = (SINGLE_ITEM_DURATION - (ItemEndTime - Time)) / SINGLE_ITEM_DURATION;

        // Smooth decay
        if (normalizedTime < 0.75)
        {

            this->patience = 1.0 - ((0.01 / 0.75) * normalizedTime);
        }
        else
        {
            // Exponential decline for the remaining 0.2 over [0.75, 1.0]
            double remainingTime = (normalizedTime - 0.75) / (1.0 - 0.75);                      // Normalize [0.9, 1.0] to [0, 1]
            this->patience = 0.99 - 0.2 * pow(remainingTime, 5);                              // Start from 0.98 and drop exponentially
            // printf("Normalized time: %.2f, patience %.2f\n", normalizedTime, this->patience); // TODO: REMOVE
        }

        // TODO: Remove ?
        // // Apply additional small adjustments if not leading
        // if (!this->isLeading)
        // {
        //     this->patience -= Normal(0.01, 0.005);
        //     if (this->patience < 0)
        //         this->patience = 0;
        // }
    }
};

// Sniping strategy
// Bids higher than the current price by minimum increment
// Sniper wait for the very last moment to bid
class SnipingBidder : public Process
{
public:
    double valuation = 0;
    double snipeDelay = 0.2; // Trying to snipe in the last 0.2 seconds

    SnipingBidder(double val) : valuation(val) {}

    void Behavior()
    {

        double snipeTime = Time + ItemEndTime - Normal(snipeDelay, (0.1 / 3)); // Latency errors
        if (Time < snipeTime)
        {
            Wait(snipeTime - Time);
        }

        if (Time > ItemEndTime)
        {
            Passivate();
        }

        if (currentPrice + minimalIncrement() <= valuation && !biddingFacility.Busy())
        {
            Seize(biddingFacility);
            Wait(Normal(0.3, (0.1 / 3))); // Reaction time + 3 sigma rule
            if (currentPrice + minimalIncrement() < this->valuation)
            {
                if (Time > ItemEndTime)
                {
                    Release(biddingFacility);
                    Passivate();
                }

                currentPrice += minimalIncrement();
                firstBidPlaced = true;
                if (LOGGING)
                {
                    logSingleBid(currentPrice);
                }
                printf("[SNIPER] placed a bid. New price: %.2f\n", currentPrice);
                lastBidder = SNIPER;
            }
            Release(biddingFacility);
        }
        else
        {
            Passivate();
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
                (new AgentBidder(RealPrice * Normal(1.2, 0.5 / 2)))->Activate(); // TODO: V dokumentacii povedat, ze nastavene podla toho, ze ebay v priemere 16 bids na aukciu
            }
            else if (probability < 0.65)
            {
                (new RatchetBidder(RealPrice * Normal(1.2, 0.5 / 2)))->Activate();
            }
            else
            {
                (new SnipingBidder(RealPrice * Normal(1.2, 0.5 / 2)))->Activate();
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
            id->Release(runningAuction);
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
        Seize(runningAuction);
        // Generate bidders
        ItemEndTime = Time + SINGLE_ITEM_DURATION;

        itemNumber++;

        // Generate the value of the item
        RealPrice = Exponential(1000 * Normal(1.0, 0.2));
        printf("Created item with value %.2f\n", RealPrice);

        lastBidder = NONE;
        printf("Resetting last bidder to %d\n", lastBidder);

        currentPrice = RealPrice * Normal(0.8, 0.2);

        // Reset the current price
        printf("Auction started for item valued at %.2f\n", currentPrice);

        // Create bidders
        (new BidderGenerator)->Activate();

        // If there are no bidders in the first 30 seconds, the item is discarded
        FirstBidTimeout *firstBidTimeout = new FirstBidTimeout(this, 30, &firstBidPlaced); // TODO: Pomocna fronta miesto boolu

        printf("This auction will end at %.2f\n", ItemEndTime);
        printf("Current time is %.2f\n", Time);
        // Wait until the end of the auction
        Wait(SINGLE_ITEM_DURATION);
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
        Release(runningAuction);
    }
};

class Auction : public Process
{
public:
    void Behavior()
    {
        while (itemNumber < NUMBER_OF_ITEMS)
        {
            Seize(runningAuction); // indicated the end of the auction for a single item
            printf("AUCTION STARTED\n");

            // Create and activate an auction item
            AuctionItem *item = new AuctionItem();
            item->Activate();

            // Wait for the next auction

            // Pause between items
            Wait(60);

            Release(runningAuction);
        }
        printf("All items sold for today\n");
    }
};

int main()
{
    RandomSeed(time(NULL));
    Init(0, (SINGLE_ITEM_DURATION * 2 + 10) * NUMBER_OF_ITEMS);
    (new Auction)->Activate();
    Run();

    // Statistics
    SetOutput("stats.out");
    printf("Simulation finished\n");
    biddingFacility.Output();
    winners.Output();
    runningAuction.Output();
}