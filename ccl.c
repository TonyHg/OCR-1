#include "ccl.h"
#include "image.h"
#include "segmentation.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdbool.h>

/**
 * Main function for segmentation with ccl process.
 * Split an image in paragraphs, then in lines, then in characters.
 */
LinkedList *ccl_segmentation(Img *source, bool whitespaces)
{
    Block *block = img_make_block(source);
    LinkedList *paragraphs = block_split_vertical(source, block, true);
    LinkedList *chars = list_init();

    Node *p = paragraphs->start;
    while (p)
    {
        // For each block, get the lines
        LinkedList *lines = line_split(source, p->data);
        Node *l = lines->start;
        while (l)
        {
            // For each line, get the characters and concatenate the result
            chars = list_concat(chars, ccl_segment_block(source, l->data));
            // Insert a whitespace betwen each line
            if (whitespaces)
            {
                Img *space = img_init(0,0);
                space->label = ' ';
                list_insert(chars, node_init(space));
            }

            l = l->next;
        }

        // Insert a new line between each block
        if (whitespaces && p->next)
        {
            Img *newline = img_init(0,0);
            newline->label = '\n';

            list_insert(chars, node_init(newline));
            list_insert(chars, node_init(newline));
        }

        // Maybe free lines with content & blocks ?
        p = p->next;
    }
    // Maybe free paragraphs with content & blocks ?
    return chars;
}

/**
 * Segment a line.
 */
LinkedList *ccl_segment_block(Img *source, Block *block)
{
    int *hist = calloc(block->width * block->height, sizeof(int));
    LinkedList *labels = create_hist(source, block, hist);
    merge_labels(block, labels, hist);
    return hist_to_images(block, labels, hist);
}

void hist_print(int *hist, int width, int height)
{
    for (int y = 0; y < height; y++)
    {
        for (int x = 0; x < width; x++)
        {
            printf("%d", hist[y * width + x]);
        }
        printf("\n");
    }
}

LinkedList *hist_to_images(Block *block, LinkedList *labels, int *hist)
{
    LinkedList *res = list_init();
    int currentLabel = 1;
    Node *charBlock = labels->start;
    while (charBlock)
    {
        Block *b = charBlock->data;
        // Skip if whitespace
        if (b->label != '\0')
        {
            Img *space = img_init(0, 0);
            space->label = b->label;
            list_insert(res, node_init(space));
            charBlock = charBlock->next;
            continue;
        }

        Img *image = img_init(b->width, b->height);
        bool found = false;
        for (int y = 0; y < b->height; y++)
        {
            for (int x = 0; x < b->width; x++)
            {
                if (hist[(y + b->y) * block->width + x + b->x] == currentLabel)
                {
                    image->pixels[y * b->width + x] = 0;
                    found = true;
                }
            }
        }
        if (found)
        {
            list_insert(res, node_init(image));
            charBlock = charBlock->next;
        }
        else
        {
            img_delete(image);
        }
        currentLabel++;
    }
    return res;
}

/**
 * Merge two blocks if one is contained in the other.
 */
void merge_labels(Block *block, LinkedList *labels, int *hist)
{
    Node *current = labels->start;
    int currentLabel = 1;
    while (current)
    {
        if (merge_two(block, labels, current->next, current, hist, currentLabel))
            currentLabel += 2;
        else
            currentLabel++;

        // Compare coordinates with the previous letter to insert a whitespace if needed.
        Block *cBlock = current->data;
        if (current->previous != NULL)
        {
            Block *pBlock = current->previous->data;
            if (pBlock != NULL && cBlock->x > pBlock->x + pBlock->width + block->height / 4)
            {
                // Create the whitespace block
                Block *space = block_init();
                space->x = pBlock->x + pBlock->width;
                space->width = cBlock->x - space->x;
                space->y = block->y;
                space->height = block->height;
                space->label = ' ';

                // Insert
                Node *new = node_init(space);
                new->previous = current->previous;
                new->previous->next = new;
                new->next = current;
                current->previous = new;
            }
        }
        current = current->next;
    }
}

/**
 * Merge the current labeled block with the next one if the second is contained in the first.
 * @param lineBlock: Block of the entire line.
 * @param labels: LinkedList of characters.
 * @param src: The next character block, which will be deleted.
 * @param dst: The current character block, which will contain the next one too.
 */
int merge_two(Block *lineBlock, LinkedList *labels, Node *src, Node *dst, int *hist, int currentLabel)
{
    if (src == NULL)
        return 0;

    Block *srcB = src->data;
    Block *dstB = dst->data;

    // We process only if srcB is contained in dstB
    // Begining of src (next) >= begining of dest (current)
    // End of src <= end of dst
    // Otherwise, return
    if (srcB->x < dstB->x || dstB->x + dstB->width + 3 < srcB->x + srcB->width)
        return 0;

    // Merge blocks
    for (int y = srcB->y; y < srcB->y + srcB->height; y++)
    {
        for (int x = srcB->x; x < srcB->x + srcB->width; x++)
        {
            // For each pixel of the source block, replace the label
            if (hist[y * lineBlock->width + x] == currentLabel + 1)
            {
                hist[y * lineBlock->width + x] = currentLabel;
            }
        }
    }

    // Update coordinates
    if (srcB->y < dstB->y)
    {
        dstB->height += dstB->y - srcB->y;
        dstB->y = srcB->y;
    }
    if (srcB->x + srcB->width > dstB->x + dstB->width)
    {
        dstB->width = srcB->width + srcB->x - dstB->x;
    }
    if (srcB->y + srcB->height > dstB->y + dstB->height)
    {
        // dstB->height += srcB->y + srcB->height - (dstB->y + dstB->height);
        dstB->height = srcB->y + srcB->height - dstB->y;
    }
    list_remove(labels, src);

    return 1;
}

void print_ccl(Img *source, LinkedList *labels)
{
    Node *n = labels->start;
    while (n)
    {
        Block *b = n->data;
        for (int y = b->y; y < b->y + b->height; y++)
        {
            for (int x = b->x; x < b->x + b->width; x++)
            {
                source->pixels[(y)*source->width + x] = 0;
            }
        }
        n = n->next;
    }

    img_save(source, "res/ccl.png");
}

LinkedList *create_hist(Img *source, Block *block, int *hist)
{
    LinkedList *labels = list_init();
    int currentLabel = 1;
    // Iterates through each pixel
    for (int x = 0; x < block->width; x++)
    {
        for (int y = 0; y < block->height; y++)
        {
            // If the pixel is black and not labeled
            if (hist[y * block->width + x] == 0 && source->pixels[(y + block->y) * source->width + x + block->x] == 0)
            {
                // Create the block that will store the size and location
                Block *current = block_init();
                current->x = x;
                current->y = y;
                // Start propagation and increment the label
                propagate(source, block, current, hist, x, y, currentLabel);
                currentLabel++;
                Node *node = node_init(current);
                list_insert(labels, node);
            }
        }
    }

    return labels;
}

/**
 * Complete a labeled content from a starting pixel
 */
void propagate(Img *source, Block *block, Block *current, int *hist, int x, int y, int currentLabel)
{
    // Check if the pixel exists and is not labeled
    if (x < 0 || x >= block->width || y < 0 || y >= block->height || hist[y * block->width + x] != 0)
        return;
    if (source->pixels[(y + block->y) * source->width + x + block->x] == 1)
        return;
    hist[y * block->width + x] = currentLabel;
    // Update block's coordinates
    if (x < current->x)
        current->x = x;
    if (y < current->y)
        current->y = y;
    if (x - current->x + 1 > current->width)
        current->width = x - current->x + 1;
    if (y - current->y + 1 > current->height)
        current->height = y - current->y + 1;
    for (int i = -1; i < 2; i++)
    {
        for (int j = -1; j < 2; j++)
        {
            if (j == 0 && i == 0)
                continue;

            propagate(source, block, current, hist, x + j, y + i, currentLabel);
        }
    }
}
