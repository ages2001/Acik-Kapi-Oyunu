#include <stdio.h>

int main() {

    int n;

    printf("Kapi sayisini giriniz: ");
    scanf("%d",&n);

    int doors[n+1];

    for(int i=0; i<n; i++) {
        doors[i] = 0;
    }

    printf("\nOncesi: \n\n");

    for(int i=0; i<n; i++) {
        printf("Kapi %d: %d\n", i+1, doors[i]);
    }

    for(int i=1; i<=n; i++) {

        printf("\n\nTur %d: \n\n",i);

        for(int j=i; j<=n; j++) {

            if (j % i == 0) {
                doors[j-1] = !doors[j-1];
            }
        }

        for(int j=0; j<n; j++) {
            printf("Kapi %d: %d\n", j+1, doors[j]);
        }
    }

    printf("\nSonrasi: \n\n");

    for(int i=0; i<n; i++) {
        printf("Kapi %d: %d\n", i+1, doors[i]);
    }

    return 0;
}
