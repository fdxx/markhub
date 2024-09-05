document.querySelectorAll('.tabs-tab-button').forEach(button => {
    button.addEventListener('click', () => {
        const tabIndex = button.getAttribute('data-tab');
        const wrapper = button.closest('.tabs-tabs-wrapper');
        const buttons = wrapper.querySelectorAll('.tabs-tab-button');
        const contents = wrapper.querySelectorAll('.tabs-tab-content');
        
        buttons.forEach(btn => {
            btn.classList.remove('active');
        });
        button.classList.add('active');
        
        contents.forEach(content => {
            content.classList.remove('active');
            if (content.getAttribute('data-index') === tabIndex) {
                content.classList.add('active');
            }
        });
    });
});
