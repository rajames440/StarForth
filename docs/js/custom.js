(function () {
    // Example: keyboard shortcut to toggle wide layout
    document.addEventListener('keydown', (e) => {
        if (e.key === 'W' && (e.ctrlKey || e.metaKey)) {
            document.body.classList.toggle('wide');
            e.preventDefault();
        }
    });
    // You can add analytics, TOC tweaks, etc. here.
})();
