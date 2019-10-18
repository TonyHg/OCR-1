#include "image.h"
#include "segmentation.h"
#include "dataset.h"
#include <dirent.h>
#include <stdlib.h>
#include <string.h>
#include <ImageMagick-7/MagickWand/MagickWand.h>

char *concat(char *a, char *b)
{
    int len = strlen(a) + strlen(b) + 1;
    char *res = malloc(sizeof(char) * len);

    strcpy(res, a);
    strcat(res, b);

    return res;
}

void dataset_to_pixels(Img **images, int dataCount)
{

    fputs("\e[?25l", stdout); /* hide the cursor */

    for (int i = 0; i < dataCount && images[i]; i++)
    {

        printf("\r%d / %d", i + 1, dataCount);
        MagickWand *mw = NewMagickWand();
        Img *image = images[i];

        if (MagickReadImage(mw, image->filepath) == MagickTrue)
        {
            // printf("File opened successfuly\n");
            // MagickAdaptiveResizeImage(mw, 28, 28);
            MagickExportImagePixels(mw, 0, 0, MagickGetImageWidth(mw), MagickGetImageHeight(mw), "R", DoublePixel, image->pixels);

            //print_image(image);
        }
        else
            printf("FAILED: %s\n", image->filepath);
        DestroyMagickWand(mw);
    }

    fputs("\e[?25h", stdout); /* show the cursor */
    printf("\n");
}
const char *string = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ'()-_.,?!:;";

/* 
 * Create an array of resized images from a list of blocks
 * 
*/
Img **images_from_list(Img *source, LinkedList *chars, int *count)
{
    int length = list_length(chars);
    Img **images = malloc(sizeof(Img) * length);
    Node *n = chars->start;

    int i = 0;
    for (i = 0; i < length && string[i] != '\0'; i++)
    {
        Img *c = img_resize(source, n->data, 28, 28);
        images[i] = c;
        images[i]->label = string[i];
        n = n->next;
    }
    *count = i;
    return images;
}

/* 
 * Open the training images of lines and create training images of chars
*/
void create_dataset_from_img(char* source, char *destination)
{
    printf("Creating dataset from images...\n");
    // Open files
    DIR *dir = opendir(source);
    struct dirent *file;
    int i = 0;
    char filepath[256];
    readdir(dir);
    readdir(dir);
    while ((file = readdir(dir)) != NULL)
    {
        // open image of the line
        sprintf(filepath, "%s/%s", source, file->d_name);
        Img *img = img_import(filepath);
        // Split into characters
        LinkedList *chars = segmentation(img);
        // Create images
        int count = 0;
        Img **images = images_from_list(img, chars, &count);
        

        // Save images
        for (int k = 0; k < count; k++)
        {
            sprintf(filepath, "%s/%u_%d.png", destination, (unsigned char)images[k]->label, i);
            img_save(images[k], filepath);
        }
        i++;
    }
}

LinkedList *read_dataset(char *filepath)
{
    /* Read the dataset without knowing number of files */

    LinkedList *images = list_init();
    DIR *dir = opendir(filepath);

    readdir(dir);
    readdir(dir);
    struct dirent *file;
    int i = 0;
    while ((file = readdir(dir)) != NULL)
    {
        // Skip hidden files
        if (file->d_name[0] == '.') continue;
        // Store the file in the img
        Img *image = img_init(28, 28);
        image->filepath = malloc(sizeof(char) * 256);
        sprintf(image->filepath, "%s/%s", filepath, file->d_name);
        int c = atoi(file->d_name); // atoi il fé de la merd
        image->label = c;

        list_insert(images, node_init(image));
        i++;
    }

    // Img ** list = (Img **)list_to_array(images);
    //dataset_to_pixels(list, i);
    return images;
}
